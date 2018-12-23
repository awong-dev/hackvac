#ifndef NVS_HANDLE_H_
#define NVS_HANDLE_H_

#include "esp_cxx/cxx17hack.h"

#include <string>

#ifndef FAKE_ESP_IDF
#include "nvs_flash.h"
#endif

#include <type_traits>

namespace esp_cxx {

// RAII class for opening up an NVS handle.
class NvsHandle {
 public:
#ifdef FAKE_ESP_IDF
  enum class Mode { kReadOnly, kReadWrite };
  using nvs_handle = void*;
#else
  enum class Mode : std::underlying_type<nvs_open_mode>::type {
    kReadOnly = NVS_READONLY,
    kReadWrite = NVS_READWRITE 
  };
#endif

  NvsHandle(const char* name, Mode mode);
  ~NvsHandle();

  NvsHandle(NvsHandle&& other);
  NvsHandle& operator=(NvsHandle&&);

  static NvsHandle OpenWifiConfig(Mode mode);
  static NvsHandle OpenOtaState(Mode mode);

  std::optional<std::string> GetString(const char* key);
  void SetString(const char* key, const std::string& value);

  std::optional<uint8_t> GetByte(const char* key);
  void SetByte(const char* key, uint8_t value);

 private:
  nvs_handle handle_ = {};

  NvsHandle(NvsHandle&) = delete;
  void operator=(NvsHandle&) = delete;
};

}  // namespace esp_cxx

#endif  // NVS_HANDLE_H_
