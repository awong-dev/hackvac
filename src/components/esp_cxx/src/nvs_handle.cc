#include "esp_cxx/nvs_handle.h"

namespace esp_cxx {

NvsHandle::NvsHandle(const char* name, nvs_open_mode mode) {
  nvs_open(name, mode, &handle_);
}

NvsHandle::~NvsHandle() {
  nvs_close(handle_);
}

NvsHandle::NvsHandle(NvsHandle&& other) : handle_(other.handle_) {
  other.handle_ = 0;
}
NvsHandle& NvsHandle::operator=(NvsHandle&& other) {
  handle_ = other.handle_;
  other.handle_ = 0;
  return *this;
}

NvsHandle NvsHandle::OpenWifiConfig(nvs_open_mode mode) {
  return NvsHandle("wifi_config", mode);
}

NvsHandle NvsHandle::OpenOtaState(nvs_open_mode mode) {
  return NvsHandle("ota_state", mode);
}

}  // namespace esp_cxx
