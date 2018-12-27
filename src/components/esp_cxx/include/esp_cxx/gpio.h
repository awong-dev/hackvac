#ifndef ESPCXX_GPIO_H_
#define ESPCXX_GPIO_H_

namespace esp_cxx {

enum gpio_num_t {
};

// Singleton class representing the configuration state for all GPIO.
class GpioConfig {
};

class Gpio {
 public:
  enum Pin {
  };

  explicit Gpio(Pin pin);
  bool Read();
  bool Write();
};

} // namespace esp_cxx

#endif  // ESPCXX_GPIO_H_
