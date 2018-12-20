#ifndef ESPCXX_WIFI_H_
#define ESPCXX_WIFI_H_

#include "esp_cxx/cxx17hack.h"

#include "esp_wifi.h"

namespace esp_cxx {

#define fldsiz(name, field) (sizeof(((name *)0)->field))
constexpr size_t kSsidBytes = fldsiz(wifi_config_t, sta.ssid);
constexpr size_t kPasswordBytes = fldsiz(wifi_config_t, sta.password);
#undef fldsize

bool LoadConfigFromNvs(
    const char fallback_ssid[], size_t fallback_ssid_len,
    const char fallback_password[], size_t fallback_password_len,
    wifi_config_t *wifi_config);

// Set and get the ssid/password from Nvs.
std::optional<std::string> GetWifiSsid();
std::optional<std::string> GetWifiPassword();

// The passed in string_views MUST be null terminated and smaller than
// kSsidBytes and kPasswordBytes respecitvely.
void SetWifiSsid(const std::string& ssid);
void SetWifiPassword(const std::string& password);

void WifiConnect(const wifi_config_t& wifi_config, bool is_station);

}  // namespace esp_cxx

#endif  // ESPCXX_WIFI_H_
