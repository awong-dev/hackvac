#ifndef ESPCXX_UART_H_
#define ESPCXX_UART_H_

#include <cstdint>
#include <cstddef>

#include "esp_cxx/gpio.h"
#include "esp_cxx/queue.h"

#ifndef FAKE_ESP_IDF
#include "driver/uart.h"

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
  using Event = uart_event_t;
  static constexpr uart_event_type_t UART_FRAME_ERR = uart_event_type_t::UART_FRAME_ERR;
  static constexpr uart_event_type_t UART_PARITY_ERR = uart_event_type_t::UART_PARITY_ERR;
  static constexpr uart_event_type_t UART_BREAK = uart_event_type_t::UART_BREAK;
  static constexpr uart_event_type_t UART_DATA_BREAK = uart_event_type_t::UART_DATA_BREAK;
  static constexpr uart_event_type_t UART_BUFFER_FULL = uart_event_type_t::UART_BUFFER_FULL;
  static constexpr uart_event_type_t UART_FIFO_OVF = uart_event_type_t::UART_FIFO_OVF;
  static constexpr uart_event_type_t UART_DATA = uart_event_type_t::UART_DATA;
  static constexpr uart_event_type_t UART_PATTERN_DET = uart_event_type_t::UART_PATTERN_DET;
  static constexpr uart_event_type_t UART_EVENT_MAX = uart_event_type_t::UART_EVENT_MAX;
#else
  enum class Chip {
    kUart0,
    kUart1,
    kUart2,
    kInvalid,
  };
enum EventType {
  UART_FRAME_ERR,
  UART_PARITY_ERR,
  UART_BREAK,
  UART_DATA_BREAK,
  UART_BUFFER_FULL,
  UART_FIFO_OVF,
  UART_DATA,
  UART_PATTERN_DET,
  UART_EVENT_MAX,
};

 
  struct Event {
    EventType type;
    size_t size;
  };
#endif

  enum class Mode {
    k8N1, // 8 data, no parity, 1 stop.
    k8E1, // 8 data, even parity, 1 stop.
  };

  Uart(Chip chip, Gpio tx_pin, Gpio rx_pin,
       int baud_rate, Mode mode);

  void Start(Queue* rx_queue, size_t rx_queue_length);

  int Read(uint8_t*buf, size_t size);
  void Write(const uint8_t* buf, size_t size);

 private:
  Chip chip_;
};

} // namespace esp_cxx

#endif  // ESPCXX_UART_H_

