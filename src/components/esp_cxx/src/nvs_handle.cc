#include "esp_cxx/nvs_handle.h"

#include <cstring>

namespace esp_cxx {

NvsHandle::NvsHandle(const char* name, Mode mode) {
#ifndef FAKE_ESP_IDF
  nvs_open(name, static_cast<nvs_open_mode>(mode), &handle_);
#endif
}

NvsHandle::~NvsHandle() {
#ifndef FAKE_ESP_IDF
  nvs_close(handle_);
#endif
}

NvsHandle::NvsHandle(NvsHandle&& other) : handle_(other.handle_) {
  other.handle_ = 0;
}
NvsHandle& NvsHandle::operator=(NvsHandle&& other) {
  handle_ = other.handle_;
  other.handle_ = 0;
  return *this;
}

NvsHandle NvsHandle::OpenWifiConfig(Mode mode) {
  return NvsHandle("wifi_config", mode);
}

NvsHandle NvsHandle::OpenOtaState(Mode mode) {
  return NvsHandle("ota_state", mode);
}

std::optional<std::string> NvsHandle::GetString(const char* key) {
  // Per ESP-IDF spec, 15 bytes w/o null terminator is max for key.
  assert(strlen(key) <= 15);
  std::optional<std::string> result;

#ifndef FAKE_ESP_IDF
  size_t stored_len;
  esp_err_t err = nvs_get_str(handle_, key, nullptr, &stored_len);
  if (err == ESP_OK) {
    result.emplace(stored_len, '\0');
    ESP_ERROR_CHECK(nvs_get_str(handle_, key, &result.value()[0], &stored_len));

    // nvs_get_str() returns a NULL terminator. Remove that so
    // std::string::size() is correct.
    result.value().resize(stored_len - 1);
  } else if (err == ESP_ERR_NVS_NOT_FOUND ||
             err == ESP_ERR_NVS_INVALID_HANDLE) {
  } else {
    ESP_ERROR_CHECK(err);
  }
#endif  // FAKE_ESP_IDF
  
  return result;
}

void NvsHandle::SetString(const char* key, const std::string& value) {
  // Per ESP-IDF spec, 15 bytes w/o null terminator is max for key.
  assert(strlen(key) <= 15);

#ifndef FAKE_ESP_IDF
  ESP_ERROR_CHECK(nvs_set_str(handle_, key, value.c_str()));
#endif  // FAKE_ESP_IDF
}

std::optional<uint8_t> NvsHandle::GetByte(const char* key) {
  std::optional<uint8_t> result;

#ifndef FAKE_ESP_IDF
  uint8_t value;
  esp_err_t err = nvs_get_u8(handle_, key, &value);
  if (err == ESP_OK) {
    result = value;
  } else if (err == ESP_ERR_NVS_NOT_FOUND) {
  } else {
    ESP_ERROR_CHECK(err);
  }
#endif

  return result;
}

void NvsHandle::SetByte(const char* key, uint8_t value) {
  // Per ESP-IDF spec, 15 bytes w/o null terminator is max for key.
  assert(strlen(key) <= 15);

#ifndef FAKE_ESP_IDF
  ESP_ERROR_CHECK(nvs_set_u8(handle_, key, value));
#endif  // FAKE_ESP_IDF
}

}  // namespace esp_cxx
