#include "half_duplex_channel.h"

#include <alloca.h>

#include "esp_cxx/logging.h"

#include "event_log.h"

namespace hackvac {

namespace {
// TODO(ajwong): Revisit queue lengths.
constexpr int kRxQueueLength = 30;
constexpr char kTag[] = "chan";
}  // namespace

HalfDuplexChannel::HalfDuplexChannel(esp_cxx::QueueSetEventManager* event_manager,
                                     esp_cxx::Uart::Chip chip,
                                     esp_cxx::Gpio tx_pin,
                                     esp_cxx::Gpio rx_pin,
                                     PacketCallback callback,
                                     PacketCallback after_send_cb,
                                     esp_cxx::Gpio tx_debug_pin,
                                     esp_cxx::Gpio rx_debug_pin)
  : event_manager_(event_manager),
    uart_(chip, tx_pin, rx_pin, 2400, esp_cxx::Uart::Mode::k8E1),
    on_packet_cb_(callback),
    after_send_cb_(callback),
    tx_debug_pin_(tx_debug_pin),
    rx_debug_pin_(rx_debug_pin) {
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
  // TODO(ajwong): Pick the right sizes and dedup constants with QueueSetHandle_t.
  uart_.Start(&rx_queue_, kRxQueueLength);
  event_manager_->Add(&rx_queue_, [this]{ OnRxEvent(); });
}

void HalfDuplexChannel::EnqueuePacket(std::unique_ptr<Cn105Packet> packet) {
  // TODO(awong): If the queue gets to be too long... should this just drop the packet?
  tx_packets_.emplace(std::move(packet));
  ScheduleSend();
}

void HalfDuplexChannel::DoSendPacket() {
  // Actually send something.
  if (!tx_packets_.empty()) {
    std::unique_ptr<Cn105Packet> packet = std::move(tx_packets_.front());
    tx_packets_.pop();
    SetTxDebug(true);
    uart_.Write(packet->raw_bytes(), packet->packet_size());
    after_send_cb_(std::move(packet));
    UpdateReadyTime();
    SetTxDebug(false);
  }

  // If there are more packets, then schedule the next send.
  if (!tx_packets_.empty()) {
    ScheduleSend();
  }
}

void HalfDuplexChannel::ScheduleSend() {
  if (Clock::now() < uart_ready_time_) {
    event_manager_->RunAfter([=]() { DoSendPacket(); }, uart_ready_time_);
  } else {
    DoSendPacket();
  }
}

void HalfDuplexChannel::DispatchRxPacket() {
  on_packet_cb_(std::move(current_rx_packet_));
  UpdateReadyTime();
  SetRxDebug(false);
}

void HalfDuplexChannel::OnRxEvent() {
  esp_cxx::Uart::Event event;
  assert(rx_queue_.Pop(&event));
  switch (event.type) {
    case esp_cxx::Uart::UART_FRAME_ERR:
    case esp_cxx::Uart::UART_PARITY_ERR:
      if (current_rx_packet_) {
        current_rx_packet_->IncrementErrorCount();
      }
      return;

    case esp_cxx::Uart::UART_BREAK:
    case esp_cxx::Uart::UART_DATA_BREAK:
    case esp_cxx::Uart::UART_BUFFER_FULL:
    case esp_cxx::Uart::UART_FIFO_OVF:
      if (current_rx_packet_) {
        current_rx_packet_->IncrementUnexpectedEventCount();
      }
      return;

    case esp_cxx::Uart::UART_DATA:
      if (!current_rx_packet_) {
        rx_packet_count_++;
        int current_packet_number = rx_packet_count_;

        // Schedule a timeout to send the packet if it hasn't finished yet.
        event_manager_->RunAfter(
            [=] {
              // If it is the same packet, this is a timeout. dispatch.
              if (current_rx_packet_ && current_packet_number == rx_packet_count_) {
                ESP_LOGI(kTag, "packet %d timed out", current_packet_number);
                DispatchRxPacket();
              }
            },
            Clock::now() + kBusyMs);
        current_rx_packet_ = std::make_unique<Cn105Packet>();
      }
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
    if (current_rx_packet_->IsJunk() && 
        buf[cursor] == Cn105Packet::kPacketStartMarker) {
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

void HalfDuplexChannel::UpdateReadyTime() {
  uart_ready_time_ = std::max(std::chrono::steady_clock::now() + kBusyMs, uart_ready_time_);
}

}  // namespace hackvac
