#include "boot_state.h"

#include "esp_cxx/nvs_handle.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_ota_ops.h"

using esp_cxx::NvsHandle;

namespace {
constexpr char kBootState[] = "boot_state";
const char *kTag = "hackvac:bootstate";
}  // namespace

namespace hackvac {

BootState GetBootState() {
  NvsHandle handle = NvsHandle::OpenBootState(NVS_READONLY);
  uint8_t value = static_cast<uint8_t>(BootState::FRESH);
  ESP_LOGE(kTag, "Got boot state: %u", value);
  esp_err_t result = nvs_get_u8(handle.get(), kBootState, &value);
  if (result != ESP_OK ||
      result != ESP_ERR_NVS_NOT_FOUND) {
    ESP_ERROR_CHECK(result);
  }

  switch (static_cast<BootState>(value)) {
    case BootState::STABLE:
      return BootState::STABLE;
    case BootState::BOOTED_ONCE:
      return BootState::BOOTED_ONCE;
    default:
      return BootState::FRESH;
  };
}

void SetBootState(BootState boot_state) {
  NvsHandle handle = NvsHandle::OpenBootState(NVS_READWRITE);
  uint8_t value = static_cast<uint8_t>(boot_state);
  
  ESP_LOGE(kTag, "Setting boot state to %u", value);
  ESP_ERROR_CHECK(nvs_set_u8(handle.get(), kBootState, value));
}

void bootstate_mark_task(void *parameters) {
  // Wait one second before writing.
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  SetBootState(BootState::STABLE);
  vTaskDelete(xTaskGetCurrentTaskHandle());
}

void firmware_watchdog() {
  const esp_partition_t* factory_partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  if (factory_partition != esp_ota_get_running_partition()) {
    BootState boot_state = GetBootState();

    // Uh oh. Rebooted before the 1 sec mark after new firmware. Return to
    // factory and reboot.
    if (boot_state == BootState::BOOTED_ONCE) {
      // TODO(awong): Return to factory and reboot.
      ESP_LOGE(kTag, "Previous firmware failed to run for 1s. Reverting to Factory Firmware.");
      ESP_ERROR_CHECK(esp_ota_set_boot_partition(factory_partition));
      esp_restart();
    }

    if (boot_state == BootState::FRESH) {
      SetBootState(BootState::BOOTED_ONCE);
      xTaskCreate(&bootstate_mark_task, "bootstate_mark_task", 4096, NULL, 1, NULL);
    }
  }
}


}  // namespace hackvac
