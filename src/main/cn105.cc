#include "cn105.h"

#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "hackvac:cn105";

static constexpr uart_port_t CN105_UART = UART_NUM_1;
static constexpr int CN105_TX_PIN = 4;
static constexpr int CN105_RX_PIN = 5;
static constexpr uart_port_t TSTAT_UART = UART_NUM_2;
static constexpr int TSTAT_TX_PIN = 17;
static constexpr int TSTAT_RX_PIN = 16;
static constexpr int BUF_SIZE = 1024; 
static constexpr int QUEUE_LENGTH = 20; 

namespace {

void write_task(void* parameters) {
  char counter = 0;
  for (;;) {
    ESP_LOGI(TAG, "cn105_sending: %u", counter);
    uart_write_bytes(CN105_UART, &counter, 1);
    counter++;
    vTaskDelay(100000 / portTICK_PERIOD_MS);
  }
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
  xTaskCreate(&write_task, "write_task", 4096, NULL, 2, NULL);
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
      uart_write_bytes(TSTAT_UART, reinterpret_cast<char*>(&buf[0]), bytes);
    }
  }
}

void Controller::TstatRxThunk(void *pvParameters) {
  static_cast<Controller*>(pvParameters)->TstatRunloop();
}

void Controller::TstatRunloop() {
  uart_event_t event;
  for (;;) {
    // Wait for an event. Send.
    if(xQueueReceive(tstat_rx_queue_, &event, (portTickType)portMAX_DELAY)) {
      static constexpr size_t buf_len = 1;
      static uint8_t buf[buf_len];
      int bytes = uart_read_bytes(TSTAT_UART, buf, buf_len, portMAX_DELAY);
      ESP_LOGI(TAG, "tstat_received: %d (%d bytes)", buf[0], bytes);
    }
  }
}

}  // namespace hackvac
