#ifndef WIFI_H_
#define WIFI_H_

#include "esp_wifi.h"

#define fldsiz(name, field) (sizeof(((name *)0)->field))

namespace hackvac {

constexpr size_t kSsidBytes = fldsiz(wifi_config_t, sta.ssid);
constexpr size_t kPasswordBytes = fldsiz(wifi_config_t, sta.password);

bool LoadConfigFromNvs(
    const char fallback_ssid[], size_t fallback_ssid_len,
    const char fallback_password[], size_t fallback_password_len,
    wifi_config_t *wifi_config);

// Set and get the ssid/password from Nvs.
bool GetWifiSsid(char* ssid, size_t* len);
bool GetWifiPassword(char* password, size_t* len);
void SetWifiSsid(const char* ssid);
void SetWifiPassword(const char* password);

void WifiConnect(const wifi_config_t& wifi_config, bool is_station);

}  // namespace hackvac

#endif  // WIFI_H_
