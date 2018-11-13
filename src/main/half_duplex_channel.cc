#include "half_duplex_channel.h"

#include <alloca.h>

#include "esp_log.h"

namespace hackvac {

namespace {
// TODO(ajwong): Revisit queue lengths.
constexpr int kRxQueueLength = 30;
constexpr int kTxQueueLength = 5;
constexpr char kTag[] = "HalfDuplexChannel";
}  // namespace

HalfDuplexChannel::HalfDuplexChannel(const char *name,
                                     uart_port_t uart,
                                     gpio_num_t tx_pin,
                                     gpio_num_t rx_pin,
                                     OnPacketCallback callback)
  : name_(name),
    uart_(uart),
    tx_pin_(tx_pin),
    rx_pin_(rx_pin),
    on_packet_cb_(callback),
    tx_queue_(xQueueCreate(kTxQueueLength, sizeof(Cn105Packet*))) {
}

HalfDuplexChannel::~HalfDuplexChannel() {
  vTaskDelete(pump_task_);
}

void HalfDuplexChannel::Start() {
  if (pump_task_) {
    ESP_LOGE(kTag, "Fatal: Channel started twice.");
    abort();
  }

  const uart_config_t uart_config = {
    .baud_rate = 2400,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_EVEN,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 40,
    .use_ref_tick = true,
  };
  uart_param_config(uart_, &uart_config);
  uart_set_pin(uart_, tx_pin_, rx_pin_, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  // TODO(ajwong): Pick the right sizes and dedup constants with QueueSetHandle_t.
  constexpr int kBufSize = 128; 
  uart_driver_install(uart_, kBufSize * 2, kBufSize * 2, kRxQueueLength, &rx_queue_, 0);

  // TODO(ajwong): priority should be passed in.
  xTaskCreate(&HalfDuplexChannel::PumpTaskThunk, name_, 4096, this, 4, &pump_task_);
}

void HalfDuplexChannel::EnqueuePacket(std::unique_ptr<Cn105Packet> packet) {
  ESP_LOGE(kTag, "Enqueue %p\n", packet.get());
//  ESP_LOG_BUFFER_HEX_LEVEL(kTag, packet->raw_bytes(), packet->packet_size(), ESP_LOG_INFO); 
  // TODO(ajwong): Should this be a blocking call or should there be at timeout?
  Cn105Packet* raw_packet = packet.release();
  xQueueSendToBack(tx_queue_, &raw_packet, (portTickType)portMAX_DELAY);
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
      xQueueSelectFromSet(queue_set, kBusyMs / portTICK_PERIOD_MS);
    if (active_member == tx_queue_) {
      Cn105Packet* packet = nullptr;
      xQueueReceive(active_member, &packet, 0);
      if (packet) {
        ESP_LOGE(kTag, "Dequeue tx packet %p\n", packet);
        tx_packets_.emplace(packet);
      }
    } else if (active_member == rx_queue_) {
      uart_event_t event;
      xQueueReceive(rx_queue_, &event, 0);
      ProcessReceiveEvent(event);
    } else {
      if (current_rx_packet_) {
        DispatchRxPacket();
      }
    }

    // If there is no |current_rx_packet_| and there are no bytes in the rx
    // buffer, then the likely channel is clear for send. Take advantage of
    // it!
    if (!current_rx_packet_) {
      /*
      size_t rx_bytes_;
      ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_, &rx_bytes_));
      if (rx_bytes_ == 0) {
        DoSendPacket();
      }
      */
      DoSendPacket();
    }
  }
}

void HalfDuplexChannel::DoSendPacket() {
  if (!tx_packets_.empty()) {
  ESP_LOGE(kTag, "Sending %p: ", tx_packets_.front().get());
  ESP_LOG_BUFFER_HEX_LEVEL(kTag, tx_packets_.front()->raw_bytes(),
                           tx_packets_.front()->packet_size(), ESP_LOG_INFO);
    uart_write_bytes(uart_, reinterpret_cast<const char*>(tx_packets_.front()->raw_bytes()),
                     tx_packets_.front()->packet_size());
    tx_packets_.pop();
    vTaskDelay(kBusyMs / portTICK_PERIOD_MS);
  }
}

void HalfDuplexChannel::DispatchRxPacket() {
  on_packet_cb_(std::move(current_rx_packet_));
  vTaskDelay(kBusyMs / portTICK_PERIOD_MS);
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
  int bytes = uart_read_bytes(uart_, buf, event.size, 0);
  if (bytes != event.size) {
    ESP_LOGE(kTag, "Unable to read all bytes in event from UART");
    abort();
  }

  // If there is no packet found yet, scan for Cn105Packet::kPacketStartMarker
  // discarding any other bytes until then.
  for (int cursor = 0; cursor < bytes; ++cursor) {
    if (!current_rx_packet_) {
      if (buf[cursor] != Cn105Packet::kPacketStartMarker) {
        ESP_LOGI(kTag, "Skip: %x\n", buf[cursor]);
        continue;
      }
      current_rx_packet_ = std::make_unique<Cn105Packet>();
    }
    current_rx_packet_->AppendByte(buf[cursor]);

    // On a completed packet, pass off and block for requisite gap
    // between sends.
    if (current_rx_packet_->IsComplete()) {
      DispatchRxPacket();
    }
  }
}

}  // namespace hackvac
