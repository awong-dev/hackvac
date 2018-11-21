#ifndef ESPCXX_OTA_H_
#define ESPCXX_OTA_H_

#include <array>

#include "esp_cxx/cxx17hack.h"
#include "mbedtls/md5.h"
#include "esp_ota_ops.h"

namespace esp_cxx {
 
enum class OtaState : int8_t {
  kFresh = 0,       // Image has just been written.
  kBootedOnce = 1,  // Image booted onetime.
  kStable = 2,      // Image has at least run for a second without crashing.
};

// Get and Sets the OtaState from NVS. Used by RunOtaWatchdog to revert to
// factory firmware on a bad OTA.
OtaState GetOtaState();
void SetOtaState(OtaState value);

// Call near startup to run a watchdog that sanity checks that an OTA does
// not enter a reboot loop. If doing OTAs, this is a good thing to always
// have.
void RunOtaWatchdog();

// Simple helper class to ensures proper opening/closing of an OTA as well
// as marking of the state.
class OtaWriter {
 public:
  OtaWriter(size_t image_size = OTA_SIZE_UNKNOWN,
            const esp_partition_t* update_partition = nullptr);
  ~OtaWriter();

  // Writes the bytes into the OTA partition.
  void Write(std::string_view chunk);

  // Finishes the write. After this md5() returns a good value.
  void Finish();

  const auto& md5() { return md5_; }

  // Sets the boot partition to |update_partition|. Can be called even if
  // OTA writing isn't complete, though that's likely a dumb idea.
  void SetBootPartition();

 private:
  mbedtls_md5_context md5_ctx_ = {};
  std::array<uint8_t, 16> md5_ = {};
  const esp_partition_t* update_partition_ = nullptr;
  esp_ota_handle_t ota_handle_ = 0;  // Per code inspection, 0 seems to invalid.
};

}  // namespace esp_cxx

#endif  // ESPCXX_OTA_H_
