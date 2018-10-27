#include "cn105.h"

#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "hackvac:cn105";

namespace {
constexpr gpio_num_t kPacPower = GPIO_NUM_4;

constexpr uart_port_t CN105_UART = UART_NUM_1;
constexpr int CN105_TX_PIN = 18;
constexpr int CN105_RX_PIN = 19;
constexpr uart_port_t TSTAT_UART = UART_NUM_2;
constexpr gpio_num_t TSTAT_TX_PIN = GPIO_NUM_5;
constexpr gpio_num_t TSTAT_RX_PIN = GPIO_NUM_17;
constexpr int BUF_SIZE = 1024; 
constexpr int QUEUE_LENGTH = 30;  // TODO(ajwong): Size to match max packet size.

constexpr char kPacketConnect[] = {
  0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa8
};
constexpr char kPacketConnectAck[] = {
  0xfc, 0x7a, 0x01, 0x30, 0x01, 0x00, 0x54, 0x00,  // This is what feels like he conn ack packet.
  0x38, 0xec, 0xfe, 0x3f, 0xd8, 0x04, 0x10, 0x40, 0xc0, 0x87, 0xfe, 0x3f, 0x00, 0x00  // But actually we need all of this.
};

void write_task(void* parameters) {
  /*
    for (;;) {
    ESP_LOGI(TAG, "cn105_sending: %u", counter);
    uart_write_bytes(CN105_UART, &counter, 1);
    counter++;
    vTaskDelay(100000 / portTICK_PERIOD_MS);
  }
  */
}

}  // namespace

namespace hackvac {

Controller::Controller()
  : is_passthru_(true) {
}

Controller::~Controller() {
  vTaskDelete( cn105_rx_task_ );
  vTaskDelete( tstat_rx_task_ );
}

void Controller::Init() {
  // Setup serial ports and map to GPIO.
  const uart_config_t uart_config = {
    .baud_rate = 2400,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_EVEN,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 40,
    .use_ref_tick = true,
  };

  gpio_pad_select_gpio(kPacPower);
  gpio_set_direction(kPacPower, GPIO_MODE_OUTPUT);
  gpio_set_level(kPacPower, 1);  // off.

  // Configure CN105 UART
  uart_param_config(CN105_UART, &uart_config);
  uart_set_pin(CN105_UART, CN105_TX_PIN, CN105_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(CN105_UART, BUF_SIZE * 2, BUF_SIZE * 2, QUEUE_LENGTH, &cn105_rx_queue_, 0);

  // Configure Tstat UART
  uart_param_config(TSTAT_UART, &uart_config);
  uart_set_pin(TSTAT_UART, TSTAT_TX_PIN, TSTAT_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(TSTAT_UART, BUF_SIZE * 2, BUF_SIZE * 2, QUEUE_LENGTH, &tstat_rx_queue_, 0);

  // Start the reading.
  xTaskCreate(&Controller::Cn105RxThunk, "cn105_rx_task", 4096, this, 4, &cn105_rx_task_);
  xTaskCreate(&Controller::TstatRxThunk, "tstat_rx_task", 4096, this, 3, &tstat_rx_task_);
//  xTaskCreate(&write_task, "write_task", 4096, NULL, 2, NULL);
}

void Controller::Cn105RxThunk(void *pvParameters) {
  static_cast<Controller*>(pvParameters)->Cn105Runloop();
}

void Controller::Cn105Runloop() {
  uart_event_t event;
  // Read MQTT intercept config.
  for (;;) {
    if(xQueueReceive(cn105_rx_queue_, &event, (portTickType)portMAX_DELAY)) {
      static constexpr size_t buf_len = 10;
      static uint8_t buf[buf_len];
      int bytes = uart_read_bytes(CN105_UART, &buf[0], buf_len, portMAX_DELAY);
      if (bytes > 0) {
        ESP_LOGI(TAG, "cn105_rx: %d bytes", bytes);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, bytes, ESP_LOG_INFO); 
        uart_write_bytes(TSTAT_UART, reinterpret_cast<char*>(&buf[0]), bytes);
      }
    }
  }
}

void Controller::TstatRxThunk(void *pvParameters) {
  static_cast<Controller*>(pvParameters)->TstatRunloop();
}

void Controller::TstatRunloop() {
  gpio_set_level(kPacPower, 0);  // PAC444CN on.
  ESP_LOGI(TAG, "PAC444CN on");

  uart_event_t event;
  size_t cur_pos = 0;
  for (;;) {
    // Wait for an event. Send.
    if(xQueueReceive(tstat_rx_queue_, &event, (portTickType)portMAX_DELAY)) {
      static constexpr size_t buf_len = 512;
      static uint8_t buf[buf_len];
      size_t size = 0;
      ESP_ERROR_CHECK(uart_get_buffered_data_len(TSTAT_UART, &size));
      if (size > buf_len) {
        ESP_LOGE(TAG, "tstat_received: way too much data %d", size);
        size = buf_len;
      }

      int bytes = uart_read_bytes(TSTAT_UART, buf, size, portMAX_DELAY);
      if (bytes > 0) {
        ESP_LOGI(TAG, "tstat_rx: %d bytes", bytes);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, buf, bytes, ESP_LOG_INFO); 
      }

      // Do connection state machine.
      for (size_t i = 0; i < bytes; ++i) {
        if (kPacketConnect[cur_pos] == buf[i]) {
          cur_pos++;
          if (cur_pos == sizeof(kPacketConnect)) {
            cur_pos = 0;
            ESP_LOGI(TAG, "~~~Sending start packet~~~");
            uart_write_bytes(TSTAT_UART, &kPacketConnectAck[0], sizeof(kPacketConnectAck));
          }
        } else {
          cur_pos = 0;
        }
      }
    }
  }
}

}  // namespace hackvac
