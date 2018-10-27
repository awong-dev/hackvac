#ifndef HALF_DUPLEX_CHANNEL_H_
#define HALF_DUPLEX_CHANNEL_H_

#include <memory>

#include "cn105_packet.h"
#include "driver/uart.h"

namespace hackvac {

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

}  // namespace hackvac
 
#endif  // HALF_DUPLEX_CHANNEL_H_
