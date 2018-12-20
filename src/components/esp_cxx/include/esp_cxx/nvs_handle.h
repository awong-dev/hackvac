#ifndef NVS_HANDLE_H_
#define NVS_HANDLE_H_

#include "esp_cxx/cxx17hack.h"

#include "nvs_flash.h"

namespace esp_cxx {

// RAII class for opening up an NVS handle.
class NvsHandle {
 public:
  NvsHandle(const char* name, nvs_open_mode mode);
  ~NvsHandle();

  NvsHandle(NvsHandle&& other);
  NvsHandle& operator=(NvsHandle&&);

  static NvsHandle OpenWifiConfig(nvs_open_mode mode);
  static NvsHandle OpenOtaState(nvs_open_mode mode);

  std::optional<std::string> GetString(const char* key);
  void SetString(const char* key, const std::string& value);

  nvs_handle get() const { return handle_; }

 private:
  nvs_handle handle_;

  NvsHandle(NvsHandle&) = delete;
  void operator=(NvsHandle&) = delete;
};

}  // namespace esp_cxx

#endif  // NVS_HANDLE_H_
