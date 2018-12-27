#include "half_duplex_channel.h"

#include <alloca.h>

#include "esp_log.h"
#include "event_log.h"

namespace hackvac {

namespace {
// TODO(ajwong): Revisit queue lengths.
constexpr int kRxQueueLength = 30;
constexpr int kTxQueueLength = 5;
constexpr char kTag[] = "chan";
}  // namespace

HalfDuplexChannel::HalfDuplexChannel(const char *name,
                                     esp_cxx::Uart::Chip chip,
                                     gpio_num_t tx_pin,
                                     gpio_num_t rx_pin,
                                     PacketCallback callback,
                                     PacketCallback after_send_cb,
                                     gpio_num_t tx_debug_pin,
                                     gpio_num_t rx_debug_pin)
  : name_(name),
    uart_(chip, tx_pin, rx_pin, 2400, esp_cxx::Uart::Mode::k8E1),
    on_packet_cb_(callback),
    after_send_cb_(callback),
    tx_debug_pin_(tx_debug_pin),
    rx_debug_pin_(rx_debug_pin),
    tx_queue_(xQueueCreate(kTxQueueLength, sizeof(Cn105Packet*))) {
  if (tx_debug_pin_ != GPIO_NUM_MAX || rx_debug_pin_ != GPIO_NUM_MAX) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask =
      ((1ULL << tx_debug_pin_) | (1ULL << rx_debug_pin_))
      & ((1ULL << GPIO_NUM_MAX) - 1);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    SetTxDebug(false);
    SetRxDebug(false);
  }
}

HalfDuplexChannel::~HalfDuplexChannel() {
  if (pump_task_) {
    vTaskDelete(pump_task_);
  }
  vQueueDelete(tx_queue_);
}

void HalfDuplexChannel::Start() {
  if (pump_task_) {
    ESP_LOGE(kTag, "Fatal: Channel started twice.");
    abort();
  }

  // TODO(ajwong): Pick the right sizes and dedup constants with QueueSetHandle_t.
  uart_.Start(&rx_queue_, kRxQueueLength);

  // TODO(ajwong): priority should be passed in.
  xTaskCreate(&HalfDuplexChannel::PumpTaskThunk, name_, 4096, this, 4, &pump_task_);
}

void HalfDuplexChannel::EnqueuePacket(std::unique_ptr<Cn105Packet> packet) {
  // TODO(ajwong): Should this be a blocking call or should there be at timeout?
  Cn105Packet* raw_packet = packet.release();
  xQueueSendToBack(tx_queue_, &raw_packet,
                   static_cast<portTickType>(portMAX_DELAY));
}

// static
void HalfDuplexChannel::PumpTaskThunk(void *pvParameters) {
  static_cast<HalfDuplexChannel*>(pvParameters)->PumpTaskRunloop();
}

void HalfDuplexChannel::PumpTaskRunloop() {
  // TODO(ajwong): Incorrect queue size.
  QueueSetHandle_t queue_set = xQueueCreateSet(kRxQueueLength + kTxQueueLength);
  xQueueAddToSet(rx_queue_, queue_set);
  xQueueAddToSet(tx_queue_, queue_set);

  for (;;) {
    QueueSetMemberHandle_t active_member = 
      xQueueSelectFromSet(queue_set, (kBusyMs * 3) / portTICK_PERIOD_MS);
    if (active_member == tx_queue_) {
      Cn105Packet* packet = nullptr;
      xQueueReceive(active_member, &packet, 0);
      if (packet) {
        tx_packets_.emplace(packet);
      }
    } else if (active_member == rx_queue_) {
      uart_event_t event;
      xQueueReceive(rx_queue_, &event, 0);
      ProcessReceiveEvent(event);
    } else {
      // This is a timeout.
      if (current_rx_packet_) {
        ESP_LOGI(kTag, "p to: %p", current_rx_packet_.get());
        DispatchRxPacket();
      }
    }

    // If there is no |current_rx_packet_| and there are no bytes in the rx
    // buffer, then the likely channel is clear for send. Take advantage of
    // it!
    //
    // TODO(awong): This is a TERRIBLE idea it turns out. If there's noise
    // in the RX line, this will starve sends.
      /*
    if (!current_rx_packet_) {
      size_t rx_bytes_;
      ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_, &rx_bytes_));
      if (rx_bytes_ == 0) {
        DoSendPacket();
      }
    }
      */
    DoSendPacket();
  }
}

void HalfDuplexChannel::DoSendPacket() {
  if (!tx_packets_.empty()) {
    std::unique_ptr<Cn105Packet> packet = std::move(tx_packets_.front());
    tx_packets_.pop();
    SetTxDebug(true);
    uart_.Write(packet->raw_bytes(), packet->packet_size());
    after_send_cb_(std::move(packet));
    vTaskDelay(kBusyMs / portTICK_PERIOD_MS);
    SetTxDebug(false);
  }
}

void HalfDuplexChannel::DispatchRxPacket() {
  on_packet_cb_(std::move(current_rx_packet_));
  SetRxDebug(false);
}

void HalfDuplexChannel::ProcessReceiveEvent(uart_event_t event) {
  switch (event.type) {
    case UART_FRAME_ERR:
    case UART_PARITY_ERR:
      if (current_rx_packet_) {
        current_rx_packet_->IncrementErrorCount();
      }

    case UART_BREAK:
    case UART_DATA_BREAK:
    case UART_BUFFER_FULL:
    case UART_FIFO_OVF:
      if (current_rx_packet_) {
        current_rx_packet_->IncrementUnexpectedEventCount();
      }
      return;

    case UART_DATA:
      break;

    case UART_PATTERN_DET:
    case UART_EVENT_MAX:
      ESP_LOGE(kTag, "Fatal: Impossible UART event.");
      abort();
  }

  // This is a data packet. Process it.
  uint8_t* buf = reinterpret_cast<uint8_t*>(alloca(event.size));
  int bytes = uart_.Read(buf, event.size);
  if (bytes != event.size) {
    ESP_LOGE(kTag, "Unable to read all bytes in event from UART");
    // TODO(awong): Does this really need to crash?
    abort();
  }

  // If there is no packet found yet, scan for Cn105Packet::kPacketStartMarker
  // discarding any other bytes until then.
  for (int cursor = 0; cursor < bytes; ++cursor) {
    // If there is no packet or the packet is junk, look for a start marker
    // and potentially create a new packet.
    if (!current_rx_packet_ ||
        (current_rx_packet_->IsJunk() && 
         buf[cursor] == Cn105Packet::kPacketStartMarker)) {
      // If there is a packet, then it was junk and that needs to be
      // dispatched without delay.
      if (current_rx_packet_) {
        DispatchRxPacket();
      }

      current_rx_packet_ = std::make_unique<Cn105Packet>();
      ESP_LOGI(kTag, "p s: %x %p", buf[cursor], current_rx_packet_.get());
      SetRxDebug(true);
    }

    // Insert byte into the new packet.
    current_rx_packet_->AppendByte(buf[cursor]);
//    ESP_LOGI(kTag, "r: %x", buf[cursor]);

    // On a completed packet, pass off and block for requisite gap
    // between sends.
    if (current_rx_packet_->IsComplete()) {
      ESP_LOGI(kTag, "p d: %p", current_rx_packet_.get());
      DispatchRxPacket();
      vTaskDelay(kBusyMs / portTICK_PERIOD_MS);
    }
  }
}

void HalfDuplexChannel::SetTxDebug(bool is_high) {
  if (tx_debug_pin_ != GPIO_NUM_MAX) {
    ESP_ERROR_CHECK(gpio_set_level(tx_debug_pin_, is_high));
  }
}

void HalfDuplexChannel::SetRxDebug(bool is_high) {
  if (rx_debug_pin_ != GPIO_NUM_MAX) {
    ESP_ERROR_CHECK(gpio_set_level(rx_debug_pin_, is_high));
  }
}

}  // namespace hackvac
