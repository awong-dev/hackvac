#include "esp_cxx/uart.h"

namespace esp_cxx {

Uart::Uart(Chip chip, Gpio tx_pin, Gpio rx_pin,
           int baud_rate, Mode mode)
  : chip_(chip) {
    (void)chip_;
#ifndef FAKE_ESP_IDF
  const uart_config_t uart_config = {
    .baud_rate = baud_rate,
    .data_bits = UART_DATA_8_BITS,
    .parity = mode == Mode::k8N1 ? UART_PARITY_DISABLE : UART_PARITY_EVEN,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 40,
    .use_ref_tick = true,
  };

  uart_param_config(static_cast<uart_port_t>(chip_), &uart_config);
  uart_set_pin(static_cast<uart_port_t>(chip_), tx_pin.underlying(), rx_pin.underlying(),
               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
#endif
}

void Uart::Start(Queue<Event>* rx_queue, size_t rx_queue_length) {
#ifndef FAKE_ESP_IDF
  // TODO(ajwong): Pick the right sizes and dedup constants with QueueSetHandle_t.
  constexpr int kBufSize = 128; 
  QueueHandle_t queue;
  uart_driver_install(static_cast<uart_port_t>(chip_), kBufSize * 2, kBufSize * 2,
                      rx_queue_length, &queue, 0);
  *rx_queue = esp_cxx::Queue<esp_cxx::Uart::Event>(queue);
#endif
}

int Uart::Read(uint8_t*buf, size_t size) {
#ifndef FAKE_ESP_IDF
  return uart_read_bytes(static_cast<uart_port_t>(chip_), buf, size, 0);
#else
  return 0;
#endif
}

void Uart::Write(const uint8_t* buf, size_t size) {
#ifndef FAKE_ESP_IDF
  uart_write_bytes(static_cast<uart_port_t>(chip_),
                   reinterpret_cast<const char*>(buf),
                   size);
#endif
}

} // namespace esp_cxx
