#include "nvs_handle.h"

namespace hackvac {

NvsHandle::NvsHandle(const char* name, nvs_open_mode mode) {
  nvs_open(name, mode, &handle_);
}

NvsHandle::~NvsHandle() {
  nvs_close(handle_);
}

NvsHandle::NvsHandle(NvsHandle&& other) : handle_(other.handle_) {
  other.handle_ = 0;
}

NvsHandle NvsHandle::OpenWifiConfig(nvs_open_mode mode) {
  return NvsHandle("wifi_config", mode);
}

}  // namespace hackvac
