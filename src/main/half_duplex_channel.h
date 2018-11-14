#ifndef HALF_DUPLEX_CHANNEL_H_
#define HALF_DUPLEX_CHANNEL_H_

#include <functional>
#include <memory>
#include <queue>

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
//       issued in quick succession, an attempt to receive will occur
//       before the next send.
//   (3) If more than 20ms (what's the right timeout?) have passed with
//       no response, then the a packet is considered received and processed
//       regardless of what the format looks like.
class HalfDuplexChannel {
// TODO(ajwong): Need algorithm for reading. Read until packet ends or timeout happens.
// Up until the header, it may timeout. After the header, we know how many ms until
// the packet is done. Then we tell if it is finished or aborted.
//
// Communication channel should be based on a mode (rx or tx) and a queue (next to send)
// with a timeout. We can be permissive on the read, but sending must always be 10ms
// or more than the previous action.
 public:
  using OnPacketCallback = std::function<void(std::unique_ptr<Cn105Packet>)>;

  // TODO(ajwong): Missing priority.
  //
  // Creaes a half-duplex channel.
  // |name| is used for logging and naming the message pumping task.
  // |uart| is the hardware uart to use.
  // |tx_pin| and |rx_pin| specify the gpio pin to use.
  // |callback| is the handler called when a packet is received, or if a byte
  //     stream times out before a full packet is parsed.  
  // |tx_debug_pin| and |rx_debug_pin| if switches high during on each packet
  //     sent or receive respectively. This allows an external logic analyzer
  //     to measuring the timing of the channel logic vs when it shows up at
  //     a uart.
  HalfDuplexChannel(const char* name,
                    uart_port_t uart,
                    gpio_num_t tx_pin,
                    gpio_num_t rx_pin,
                    OnPacketCallback callback,
                    gpio_num_t tx_debug_pin = GPIO_NUM_MAX,
                    gpio_num_t rx_debug_pin = GPIO_NUM_MAX);
  ~HalfDuplexChannel();

  // Starts sending/receiving data from the UART. After this, |on_packet_cb_|
  // will begin to receive Cn105Packets.
  void Start();

  // Enqueues a packet for sending.
  void EnqueuePacket(std::unique_ptr<Cn105Packet> packet);

 private:
  // Simple thunk to adapt the C-style FreeRTOS API to C++.
  static void PumpTaskThunk(void *pvParameters);

  // Runloop for the task that manages sending/receiving packets from
  // |uart_| while maintaining the guarantees of the HalfDuplexChannel.
  void PumpTaskRunloop();

  // Synchronously sends 1 packet from |tx_packets_| to the uart_ and blocks
  // the requisite time until the channel can send/receive again.
  void DoSendPacket();

  // Synchronously invokes the |on_packet_cb_| for |current_rx_packet_| and
  // blocks the requisite time for the channel to go quiet. This effectively
  // resets the channel.
  void DispatchRxPacket();

  // Reads data from UART attempting to complete a Cn105Packet. When a packet
  // is complete, it is sent off to the |on_packet_cb_| callback, the task is
  // blocked until the channel is ready for sending/receive again, and then
  // the function returns true. If the UART runs out of data before the packet
  // is complete, return false.
  void ProcessReceiveEvent(uart_event_t event);

  // Sets the |tx_debug_pin_| to |is_high|.
  void SetTxDebug(bool is_high);

  // Sets the |rx_debug_pin_| to |is_high|.
  void SetRxDebug(bool is_high);

  // The delay to wait between the last send or last receive before the next
  // packet is allowed to be sent. Seems to be about 4 bytes worth of time.
  //
  // This is also being used as the timeout for a receive channel going dead.
  static constexpr int kBusyMs = 10;

  // Name of the channel. Used for the pump task name.
  const char* name_ = nullptr;

  // UART to read from.
  uart_port_t uart_ = UART_NUM_MAX;

  // Pin for UART TX.
  gpio_num_t tx_pin_ = GPIO_NUM_MAX;

  // Pin for UART RX.
  gpio_num_t rx_pin_ = GPIO_NUM_MAX;

  // Callback to invoke when a packet is completed.
  OnPacketCallback on_packet_cb_;

  // Pin to hold high when sending a packet.
  gpio_num_t tx_debug_pin_ = GPIO_NUM_MAX;

  // Pin to hold high when receiving a packet.
  gpio_num_t rx_debug_pin_ = GPIO_NUM_MAX;

  // Queue that receives the data from the UART.
  QueueHandle_t rx_queue_ = nullptr;

  // Binary Semaphore that signals a send is wanted.
  QueueHandle_t tx_queue_ = nullptr;

  // Current packet being received.
  std::unique_ptr<Cn105Packet> current_rx_packet_;

  // The next packet to send.
  std::queue<std::unique_ptr<Cn105Packet>> tx_packets_;

  // Task responsible for reading/sending packets.
  TaskHandle_t pump_task_ = nullptr;
};

}  // namespace hackvac
 
#endif  // HALF_DUPLEX_CHANNEL_H_
