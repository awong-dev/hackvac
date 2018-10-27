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
  void SetOnPacketHandler(OnPacketCallback callback) {
    on_packet_cb_ = callback;
  }

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

  static void PumpTaskThunk(void *pvParameters);

  void PumpTaskRunloop();

  // Handle state transitions.
  void TransitionState(QueueSetMemberHandle_t active_member);

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
