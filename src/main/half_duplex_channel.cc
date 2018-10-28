#include "half_duplex_channel.h"

namespace hackvac {
HalfDuplexChannel::HalfDuplexChannel() {
}

HalfDuplexChannel::~HalfDuplexChannel() {
}

int HalfDuplexChannel::SendPacket(const Cn105Packet& packet) {
  // Enqueue it.
}

// static
void HalfDuplexChannel::PumpTaskThunk(void *pvParameters) {
  static_cast<HalfDuplexChannel*>(pvParameters)->PumpTaskRunloop();
}

void HalfDuplexChannel::PumpTaskRunloop() {
  constexpr int QUEUE_LENGTH = 30;
  // TODO(ajwong): Incorrect queue size.
  QueueSetHandle_t queue_set = xQueueCreateSet(QUEUE_LENGTH + 1);
  xQueueAddToSet(rx_queue_, queue_set);
  xQueueAddToSet(tx_queue_, queue_set);

  Cn105Packet packet;
  for (;;) {
    QueueSetMemberHandle_t active_member = 
      xQueueSelectFromSet(queue_set, 10 / portTICK_PERIOD_MS);
    if (active_member == tx_queue_) {
      Cn105Packet* packet = nullptr;
      xQueueReceive(active_member, &packet, 0);
      if (packet) {
        tx_packets_.emplace_back(std::make_unique(packet));
      }
    } else if (active_member == rx_queue_) {
      state_ = ChannelState::RECEIVING;
      // If a the packet is complete, then move to ready state again.
      if (DoReceivePacket()) {
        state_ = ChannelState::READY;
      }
    } else {
      // TODO(ajwong): This is a timeout. Recover error. Delay. Then ready.
    }

    // If in ready state, either we were in READY and got a packet, or
    // we were in RECEIVING and it completed a packet while there was a
    // delayed one.
    if (state_ == ChannelState::READY) {
      DoSendPacket();
    }
  }
}

void HalfDuplexChannel::DoSendPacket() {
  uart_write_bytes(uart_, "TODO(Ajwong): Fix me", 3 /* Bytes */);
  vTaskDelay(kBusyMs / portTICK_PERIOD_MS);
}

}  // namespace hackvac
