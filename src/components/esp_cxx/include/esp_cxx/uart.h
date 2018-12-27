#ifndef ESPCXX_UART_H_
#define ESPCXX_UART_H_

#include <cstdint>
#include <cstddef>

#include "esp_cxx/gpio.h"

#ifndef FAKE_ESP_IDF
#include "driver/uart.h"
#else 
typedef void* QueueHandle_t;
#endif


namespace esp_cxx {

class Uart {
 public:
#ifndef FAKE_ESP_IDF
  enum class Chip {
    kUart0 = UART_NUM_0,
    kUart1 = UART_NUM_1,
    kUart2 = UART_NUM_2,
    kInvalid = UART_NUM_MAX,
  };
#else
  enum class Chip {
    kUart0,
    kUart1,
    kUart2,
    kInvalid,
  };
#endif

  enum class Mode {
    k8N1, // 8 data, no parity, 1 stop.
    k8E1, // 8 data, even parity, 1 stop.
  };

  Uart(Chip chip, gpio_num_t tx_pin, gpio_num_t rx_pin,
       int baud_rate, Mode mode);

  void Start(QueueHandle_t* rx_queue, size_t rx_queue_length);

  int Read(uint8_t*buf, size_t size);
  void Write(const uint8_t* buf, size_t size);

 private:
  Chip chip_;
};

} // namespace esp_cxx

#endif  // ESPCXX_UART_H_

