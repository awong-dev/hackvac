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

void HalfDuplexChannel::TransitionState(QueueSetMemberHandle_t active_member) {
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

}  // namespace hackvac
