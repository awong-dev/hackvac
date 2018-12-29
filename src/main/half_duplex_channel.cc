#include "half_duplex_channel.h"

#include <alloca.h>

#include "esp_cxx/logging.h"

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
                                     esp_cxx::Gpio tx_pin,
                                     esp_cxx::Gpio rx_pin,
                                     PacketCallback callback,
                                     PacketCallback after_send_cb,
                                     esp_cxx::Gpio tx_debug_pin,
                                     esp_cxx::Gpio rx_debug_pin)
  : name_(name),
    uart_(chip, tx_pin, rx_pin, 2400, esp_cxx::Uart::Mode::k8E1),
    on_packet_cb_(callback),
    after_send_cb_(callback),
    tx_debug_pin_(tx_debug_pin),
    rx_debug_pin_(rx_debug_pin),
    tx_queue_(kTxQueueLength, sizeof(Cn105Packet*)) {
  if (tx_debug_pin_ || rx_debug_pin_) {
    /*
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask =
      ((1ULL << tx_debug_pin_.underlying()) | (1ULL << rx_debug_pin_.underlying()))
      & ((1ULL << GPIO_NUM_MAX) - 1);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    SetTxDebug(false);
    SetRxDebug(false);
    */
  }
}

HalfDuplexChannel::~HalfDuplexChannel() {
}

void HalfDuplexChannel::Start() {
  if (pump_task_) {
    ESP_LOGE(kTag, "Fatal: Channel started twice.");
    abort();
  }

  // TODO(ajwong): Pick the right sizes and dedup constants with QueueSetHandle_t.
  uart_.Start(&rx_queue_, kRxQueueLength);

  // TODO(ajwong): priority should be passed in. Reevaluate stack size.
  pump_task_ = esp_cxx::Task::Create<HalfDuplexChannel, &HalfDuplexChannel::PumpTaskRunloop>(this, name_, 4096, 4);
}

void HalfDuplexChannel::EnqueuePacket(std::unique_ptr<Cn105Packet> packet) {
  // TODO(ajwong): Should this be a blocking call or should there be at timeout?
  Cn105Packet* raw_packet = packet.release();
  tx_queue_.Push(&raw_packet, esp_cxx::Queue::kMaxWait);
}

void HalfDuplexChannel::PumpTaskRunloop() {
  // TODO(ajwong): Incorrect queue size.
  esp_cxx::QueueSet queue_set(kRxQueueLength + kTxQueueLength);
  queue_set.Add(&rx_queue_);
  queue_set.Add(&tx_queue_);

  for (;;) {
    esp_cxx::Queue::Id active_member = queue_set.Select(kBusyMs * 3);
    if (tx_queue_.IsId(active_member)) {
      Cn105Packet* packet = nullptr;
      tx_queue_.Pop(&packet);
      if (packet) {
        tx_packets_.emplace(packet);
      }
    } else if (rx_queue_.IsId(active_member)) {
      esp_cxx::Uart::Event event;
      rx_queue_.Pop(&event);
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
    esp_cxx::Task::Delay(kBusyMs);
    SetTxDebug(false);
  }
}

void HalfDuplexChannel::DispatchRxPacket() {
  on_packet_cb_(std::move(current_rx_packet_));
  SetRxDebug(false);
}

void HalfDuplexChannel::ProcessReceiveEvent(esp_cxx::Uart::Event event) {
  switch (event.type) {
    case esp_cxx::Uart::UART_FRAME_ERR:
    case esp_cxx::Uart::UART_PARITY_ERR:
      if (current_rx_packet_) {
        current_rx_packet_->IncrementErrorCount();
      }

    case esp_cxx::Uart::UART_BREAK:
    case esp_cxx::Uart::UART_DATA_BREAK:
    case esp_cxx::Uart::UART_BUFFER_FULL:
    case esp_cxx::Uart::UART_FIFO_OVF:
      if (current_rx_packet_) {
        current_rx_packet_->IncrementUnexpectedEventCount();
      }
      return;

    case esp_cxx::Uart::UART_DATA:
      break;

    case esp_cxx::Uart::UART_PATTERN_DET:
    case esp_cxx::Uart::UART_EVENT_MAX:
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
      esp_cxx::Task::Delay(kBusyMs);
    }
  }
}

void HalfDuplexChannel::SetTxDebug(bool is_high) {
  if (tx_debug_pin_) {
    tx_debug_pin_.Set(is_high);
  }
}

void HalfDuplexChannel::SetRxDebug(bool is_high) {
  if (rx_debug_pin_) {
    rx_debug_pin_.Set(is_high);
  }
}

}  // namespace hackvac
