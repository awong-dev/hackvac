#ifndef NVS_HANDLE_H_
#define NVS_HANDLE_H_

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
    static NvsHandle OpenBootState(nvs_open_mode mode);

    nvs_handle get() const { return handle_; }

  private:
    nvs_handle handle_;

    NvsHandle(NvsHandle&) = delete;
    void operator=(NvsHandle&) = delete;
};

}  // namespace esp_cxx

#endif  // NVS_HANDLE_H_
