#ifndef CN105_H_
#define CN105_H_

#include <array>
#include <memory>

#include "driver/uart.h"

// This class is designed to control a Mitsubishi CN105 serial control
// interface. It can either MiTM a signal to log/modify commands, or it
// can just take over and send its own commands.
//
// CN105 is a custom checksumed protocol transported over a 2400 BAUD serial
// connection.
//
// ?? The protocol seems to be half-duplex, needing a 10ms pause between send
// ?? and receive.
//
// This class configures 2 UARTs with the expectation that it is sitting
// between the CN105 connector on the Heatpump control board, and the
// Mitsubishi-PAC-US444CN-1 Mitsubishi Thermostat Interface (tstat).
//
// The tstat is an optional client. If it isn't connected, no big deal as
// this module is the true client.
//
// Both UARTs are configured in event mode with the ESP-IDF uart drivers
// placing bytes into FreeRTOS event queues. Each of these queues is processed
// by their own FreeRTOS task. The tasks parses the messages into a CN105
// packet and forwards them along.
//
// At the start of each packet, the task with take an atomic snapshot of the
// Controller object's packet modification configuration. That ensures that each
// packet is internally consistent. It also means updates to the Controller object
// only get latched into effect at packet boundaries.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace hackvac {

// The Cn105 serial protocol sends compact, checksumed, byte-oriented packets.
//
// Each packet has the following format:
//  | 0xfc | type | 0x01 | 0x30 | data_len | ...data...  | checksum |
//     0      1      2      3      4          5             last
//  type, data_len, and checksum are all 1 byte.
//
//  0xfc is the start-byte. Useful for synchronization on corruption.
//
//  The 0x01 and 0x30 are unknown extra tags that look constant.
//  Possibly version information or other synchronization markers that can
//  detect endianess?
//
// Reference code:
//   https://github.com/hadleyrich/MQMitsi/blob/master/mitsi.py
class Cn105Packet {
  public:
    // Bit 6 seems to indicate if it is an ACK. So if 0x5a is a packet then
    // 0x7a is its ack.
    enum class PacketType : uint8_t {
      // TODO(awong): the second nibble might be zone info. a == zone 1, b = zone2, etc.
      kConnect = 0x5a,
      kConnectAck = 0x7a, 

      // Update HVAC control status.
      kUpdate = 0x41, 
      kUpdateAck = 0x61, 

      // Requesting info from the HeatPump.
      kInfo = 0x42, 
      kInfoAck = 0x62, 

      kUnknown = 0x00,
    };

    // For UPDATE (0x41), the first data byte is the setting to change.
    //   Control Update Extended
    //   0x07 = set current room temperature
    //            byte0 = room temp 0x1, 
    //            byte1 = ???
    //            room temp is first byte after.
    //            NOTE: MHK1 sends 0x80 for data if setting is nonsense (negative)
    //                  yielding 0x00 on byte0 bitflag.
    //
    //   Control Update
    //   0x01 = update all standard settings. Next 2 bytes are bitfields.
    //            byte0 = power 0x1, mode 0x2, temp 0x4, fan 0x8, vane 0x10, dir 0x80
    //            byte1 = wiadevane 0x1 
    //          Data for each is in a corresponding byte.
    //            Power = data + 3
    //            Mode = data + 4
    //            Temp = data + 5
    //            Fan = data + 6
    //            Vane = data + 7
    //            Dir = data + 10

    Cn105Packet();

    // Reset the packet to uninitialized state.
    void Reset() {
      bytes_read_ = 0;
    }

    // Returns true if byte succeeded. This can fail if the packet is complete,
    // or if the the packet length was exceeded.
    bool AppendByte(uint8_t byte) {
      if (IsComplete() || bytes_read_ >= bytes_.size()) {
        return false;
      }
      if (bytes_read_ >= bytes_.size()) {
        return false;
      }
      bytes_.at(bytes_read_++) = byte;
      return true;
    }

    // Size constants.
    static constexpr size_t kHeaderLength = 5;
    static constexpr size_t kChecksumSize = 1;

    // Packet field constants.
    static constexpr size_t kStartMarkerPos = 0;
    static constexpr size_t kTypePos = 1;
    static constexpr size_t kDataLenPos = 4;

    // Returns true if the header has been read. After this,
    // packet type and data length can be read.
    bool IsHeaderComplete() const {
      return bytes_read_ >= kHeaderLength;
    }

    size_t NextChunkSize() {
      if (!IsHeaderComplete())  {
        return kHeaderLength - bytes_read_;
      }
      return kHeaderLength + data_size() + kChecksumSize - bytes_read_;
    }

    // Header accessors. Returns valid data when IsHeaderComplete() is true.
    size_t data_size() const { return bytes_[kDataLenPos]; }
    PacketType type() const { return static_cast<PacketType>(bytes_[kTypePos]); }
    size_t packet_size() const { return kHeaderLength + data_size() + kChecksumSize; }

    // Returns true if current packet is complete.
    bool IsComplete() const {
      if (!IsHeaderComplete()) {
        return false;
      }
      return (bytes_read_ >= packet_size());
    }

    // Verifies the checksum on the packet. The checksum algorithm, based on
    // reverse-engineering packet captures from CN105 to a PAC444CN, is:
    // checksum = (0xfc - sum(data)) & 0xff
    bool IsChecksumValid() {
      uint32_t checksum = 0xfc;
      for (size_t i = 0; i < (bytes_read_ - 1); ++i) {
        checksum -= bytes_.at(i);
      }

      return checksum == bytes_.at(bytes_read_);
    }

    // https://github.com/SwiCago/HeatPump assumes 22 byte max for full packet
    // but format-wise, data_len can be 255 so max packet size may be 261.
    // However that would be wasteful in memory usage so rouding up to 30.
    constexpr static size_t kMaxPacketLength = 30;

  private:
    std::array<uint8_t, kMaxPacketLength> bytes_;
    size_t bytes_read_;
};


// This class implements a Half-Duplex packet-oriented serial channel.
//
// Looking at the data captures from the CN105 interface, it seems like
// the CN105 never initiates communication. When communication occurs,
// it is request/ack packet protocol with about a 10ms delay between
// packets.
//
// Given that the UARTs on the esp32 are full-duplex, this software stack
// attempts to fake this half-duplex setup. Guarantees are as follows:
//
//   (1) Any packet send will be delayed until 10ms after the most
//       recent packet read/send completion.
//   (2) Packet sending/receiving expects to take turns. If 2 sends are
//       issued in quick succession, the second send will fail if
//       a response has not been heard.
//   (3) If more than 20ms (what's the right timeout?) have passed with
//       no response, then the channel is considered reset and sends may
//       again occur.
class HalfDuplexChannel {
// TODO(ajwong): Need algorithm for reading. Read until packet ends or timeout happens.
// Up until the header, it may timeout. After the header, we know how many ms until
// the packet is done. Then we tell if it is finished or aborted.
//
// Communication channel should be based on a mode (rx or tx) and a queue (next to send)
// with a timeout. We can be permissive on the read, but sending must always be 10ms
// or more than the previous action.
 public:
  HalfDuplexChannel();
  ~HalfDuplexChannel();

  using OnPacketCallback = void(*)(const Cn105Packet& packet);
  void SetOnPacketHandler(OnPacketCallback callback);

  // Tries to send packet.
  //
  // Returns 0 if the packet was sent. Otherwise, returns the number of ms to
  // wait before the channel is free for sending.
  int SendPacket(const Cn105Packet& packet);

 private:
  enum class ChannelState {
    READY,
    BUSY,
    SENDING,
    RECEIVING,
  };

  static void PumpTaskThunk(void *pvParameters) {
    static_cast<HalfDuplexChannel*>(pvParameters)->PumpTaskRunloop();
  }

  void PumpTaskRunloop() {
    constexpr int QUEUE_LENGTH = 30;
    QueueSetHandle_t queue_set = xQueueCreateSet(QUEUE_LENGTH + 1);
    xQueueAddToSet(rx_queue_, queue_set);
    xQueueAddToSet(send_signal_, queue_set);

    Cn105Packet packet;
    for (;;) {
      QueueSetMemberHandle_t active_member = 
        xQueueSelectFromSet(queue_set, 10 / portTICK_PERIOD_MS);

      TransitionState(active_member);
      if (state_ == ChannelState::RECEIVING) {
        uart_event_t event;
        xQueueReceive(active_member, &event, 0);
        switch (event.type) {
          case UART_DATA:
            {
              const size_t buf_len = std::min(event.size, packet.NextChunkSize());
              uint8_t *buf = static_cast<uint8_t*>(alloca(buf_len));
              int bytes = uart_read_bytes(uart_, &buf[0], buf_len, 0);
              // TODO(ajwong): This API is dumb. Fix.
              for (int i = 0; i < bytes; ++i) {
                packet.AppendByte(buf[i]);
              }
              break;
            }
          case UART_FRAME_ERR:
          case UART_PARITY_ERR:
          case UART_DATA_BREAK:
          case UART_BREAK:
          case UART_PATTERN_DET:
            // TODO(ajwong): These are all errors. Reset the state.
          default:
            break;
        }
        if (packet.IsComplete()) {
          on_packet_cb_(packet);
          packet.Reset();
          TransitionState(active_member);
        }
      } else if (state_ == ChannelState::SENDING) {
        uart_write_bytes(uart_, "TODO(Ajwong): Fix me", 3 /* Bytes */);
          TransitionState(active_member);
      }
      /*
      if (bytes > 0) {
        ESP_LOGI(TAG, "rx: %d bytes", bytes);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, bytes, ESP_LOG_INFO); 
        uart_write_bytes(TSTAT_UART, reinterpret_cast<char*>(&buf[0]), bytes);
      }
      */
    }
  }

  // Handle state transitions.
  void TransitionState(QueueSetMemberHandle_t active_member) {
    switch (state_) {
      case ChannelState::READY:
        // Transition to SENDING or RECEIVING based on which status.
        if (active_member == rx_queue_) {
          state_ = ChannelState::RECEIVING;
        } else if (active_member == send_signal_) {
          state_ = ChannelState::SENDING;
        } else {
          // Timeout wake. Stay in READY state.
        }
        break;

      case ChannelState::BUSY:
        // Sends can be starved if there is a stream of receives. That makes
        // sense as this is a the lower-priority device in the chain between
        // the HVAC control board, this device, and the PAC444CN.
        if (active_member == rx_queue_) {
          // Transition to RECEIVING if any data is coming.
          state_ = ChannelState::RECEIVING;
        } else {
          // If timeout has been reached then check if there is a queued
          // packet.
          if (esp_timer_get_time() > not_busy_timestamp_) {
            if (packet_to_send_) {
              state_ = ChannelState::SENDING;
            } else {
              state_ = ChannelState::READY;
            }
          }
        }
        break;

      case ChannelState::SENDING:
        if (bytes_sent_ == packet_to_send_->data_size()) {
          not_busy_timestamp_ = esp_timer_get_time() + kBusyMs;
          state_ = ChannelState::BUSY;
        }
        // TODO(ajwong): Is there a reset if the UART is jammed?
        break;

      case ChannelState::RECEIVING:
        if (last_received_packet_.IsComplete()) {
          not_busy_timestamp_ = esp_timer_get_time() + kBusyMs;
          state_ = ChannelState::BUSY;
        }
        break;
    }
  }

  // The state of the channel. You can only send in the READY state.
  ChannelState state_ = ChannelState::READY;

  // Callback to invoke when a packet is completed.
  OnPacketCallback on_packet_cb_ = nullptr;

  // Timestamp of when the next send is allowed.
  int64_t not_busy_timestamp_ = 0;

  // UART to read from.
  uart_port_t uart_ = UART_NUM_MAX;

  // Queue that receives the data from the UART.
  QueueHandle_t rx_queue_ = nullptr;

  // Binary Semaphore that signals a send is wanted.
  QueueHandle_t send_signal_ = nullptr;

  // Last packet recieved.
  Cn105Packet last_received_packet_;

  // The delay to wait between the last send or last receive before the next
  // packet is allowed to be sent.
  static constexpr int kBusyMs = 20;

  // The next packet to send.
  std::unique_ptr<Cn105Packet> packet_to_send_;

  // Tracks how many bytes to have been sent from |packet_to_send_|.
  size_t bytes_sent_ = 0;

  // Task responsible for reading/sending packets.
  TaskHandle_t pump_task_ = nullptr;
};
 
class Controller {
  public:
    Controller();
    ~Controller();

    void Init();

    // Mode changes.
    void set_passthru(bool is_passthru) { is_passthru_ = is_passthru; }
    bool is_passthru() { return is_passthru_; }

  private:
    static void Cn105RxThunk(void *pvParameters);
    void Cn105Runloop();

    static void TstatRxThunk(void *pvParameters);
    void TstatRunloop();

    bool is_passthru_;

    TaskHandle_t cn105_rx_task_;
    QueueHandle_t cn105_rx_queue_;

    TaskHandle_t tstat_rx_task_;
    QueueHandle_t tstat_rx_queue_;
};

}  // namespace hackvac

#endif  // CN105_H_
