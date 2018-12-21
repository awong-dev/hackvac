#include "esp_cxx/ota.h"

#include "esp_cxx/logging.h"
#include "esp_cxx/nvs_handle.h"

#ifndef FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_ota_ops.h"
#endif  // FAKE_ESP_IDF

namespace esp_cxx {

namespace {

constexpr char kOtaStateKey[] = "ota_state";

#ifndef FAKE_ESP_IDF
void bootstate_mark_task(void *parameters) {
  // Wait one second before writing.
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  SetOtaState(OtaState::kStable);
  vTaskDelete(xTaskGetCurrentTaskHandle());
}
#endif

}  // namespace

OtaState GetOtaState() {
  NvsHandle handle = NvsHandle::OpenOtaState(NvsHandle::Mode::kReadOnly);
  uint8_t value = handle.GetByte(kOtaStateKey).value_or(
      static_cast<uint8_t>(OtaState::kFresh));
  ESP_LOGE(kEspCxxTag, "Got ota state: %u", value);

  switch (static_cast<OtaState>(value)) {
    case OtaState::kStable:
      return OtaState::kStable;
    case OtaState::kBootedOnce:
      return OtaState::kBootedOnce;
    default:
      return OtaState::kFresh;
  };
}

void SetOtaState(OtaState ota_state) {
  NvsHandle handle = NvsHandle::OpenOtaState(NvsHandle::Mode::kReadWrite);
  uint8_t value = static_cast<uint8_t>(ota_state);
  
  ESP_LOGE(kEspCxxTag, "Setting ota state to %u", value);
  handle.SetByte(kOtaStateKey, value);
}

void RunOtaWatchdog() {
#ifndef FAKE_ESP_IDF
  const esp_partition_t* factory_partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
  if (factory_partition != esp_ota_get_running_partition()) {
    OtaState ota_state = GetOtaState();

    // Uh oh. Rebooted before the 1 sec mark after new firmware. Return to
    // factory and reboot.
    if (ota_state == OtaState::kBootedOnce) {
      // TODO(awong): Return to factory and reboot.
      ESP_LOGE(kEspCxxTag, "Previous firmware failed to run for 1s. Reverting to Factory Firmware.");
      ESP_ERROR_CHECK(esp_ota_set_boot_partition(factory_partition));
      esp_restart();
    }

    if (ota_state == OtaState::kFresh) {
      SetOtaState(OtaState::kBootedOnce);
      xTaskCreate(&bootstate_mark_task, "bootstate_mark_task", 4096, NULL, 1, NULL);
    }
  }
#endif
}

#ifdef FAKE_ESP_IDF
OtaWriter::OtaWriter(size_t image_size, const esp_partition_t* update_partition) {
}
#else
OtaWriter::OtaWriter(size_t image_size, const esp_partition_t* update_partition)
    : update_partition_(update_partition) {
  if (!update_partition_) {
    update_partition_ = esp_ota_get_next_update_partition(nullptr);
  }
  mbedtls_md5_init(&md5_ctx_);
  mbedtls_md5_starts(&md5_ctx_);
  ESP_ERROR_CHECK(esp_ota_begin(update_partition_, image_size, &ota_handle_));
}
#endif

OtaWriter::~OtaWriter() {
#ifndef FAKE_ESP_IDF
  if (ota_handle_) {
    ESP_ERROR_CHECK(esp_ota_end(ota_handle_));
  }
#endif
}

void OtaWriter::Write(std::string_view chunk) {
#ifndef FAKE_ESP_IDF
  mbedtls_md5_update(&md5_ctx_,
                     reinterpret_cast<const unsigned char*>(chunk.data()),
                     chunk.size());
  ESP_ERROR_CHECK(esp_ota_write(ota_handle_, chunk.data(), chunk.size()));
#endif
}

void OtaWriter::Finish() {
#ifndef FAKE_ESP_IDF
  mbedtls_md5_finish(&md5_ctx_, &md5_[0]);
  mbedtls_md5_free(&md5_ctx_);
  ESP_ERROR_CHECK(esp_ota_end(ota_handle_));
  ota_handle_ = 0;
#endif
}

void OtaWriter::SetBootPartition() {
#ifndef FAKE_ESP_IDF
  SetOtaState(OtaState::kFresh);
  ESP_ERROR_CHECK(esp_ota_set_boot_partition(update_partition_));
#endif
}

}  // namespace esp_cxx
