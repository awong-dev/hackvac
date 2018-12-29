#ifndef ESPCXX_GPIO_H_
#define ESPCXX_GPIO_H_

#ifndef FAKE_ESP_IDF
#include "driver/gpio.h"
#endif

namespace esp_cxx {

// Singleton class representing the configuration state for all GPIO.
class GpioConfig {
};

class Gpio {
 public:
  static constexpr int kMaxGpio = 40;

#ifndef FAKE_ESP_IDF
  static_assert(kMaxGpio == GPIO_NUM_MAX, "Gpio Max is incorrect");
#endif
  Gpio();

  template <int n>
  static constexpr Gpio Pin() {
    static_assert(n >= 0 && n < kMaxGpio, "Invalid Gpio number");
    return Gpio(n);
  }

  bool Get() const;
  void Set(bool is_high) const;

  explicit operator bool() const { return pin_ != kMaxGpio; }

#ifndef FAKE_ESP_IDF
  gpio_num_t underlying() const { return static_cast<gpio_num_t>(pin_); }
#else
  int underlying() const { return pin_; }
#endif

 private:
  explicit constexpr Gpio(int pin) : pin_(pin) {}

  int pin_;
#ifdef FAKE_ESP_IDF
  mutable bool fake_state_ = false;
#endif
};

} // namespace esp_cxx

#endif  // ESPCXX_GPIO_H_
