#ifndef BOOT_STATE_H_
#define BOOT_STATE_H_

#include "esp_types.h"

namespace hackvac {
 
enum class BootState : int8_t {
  FRESH = 0,
  BOOTED_ONCE = 1,
  STABLE = 2,
};

BootState GetBootState();
void SetBootState(BootState value);
void firmware_watchdog_task(void *parameters);

}  // namespace hackvac

#endif  // BOOT_STATE_H_
