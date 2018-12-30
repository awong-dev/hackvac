#include "esp_cxx/gpio.h"

namespace esp_cxx {

Gpio::Gpio() = default;

bool Gpio::Get() const {
#ifndef FAKE_ESP_IDF
  return gpio_get_level(underlying());
#else
  return fake_state_;
#endif
}

void Gpio::Set(bool is_high) const {
#ifndef FAKE_ESP_IDF
  ESP_ERROR_CHECK(gpio_set_level(underlying(), is_high));
#else
  fake_state_ = is_high;
#endif
}

}  // namespace esp_cxx
