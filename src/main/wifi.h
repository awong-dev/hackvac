#ifndef WIFI_H_
#define WIFI_H_

#include "esp_wifi.h"

namespace hackvac {

bool LoadConfigFromNvs(
    const char fallback_ssid[], size_t fallback_ssid_len,
    const char fallback_password[], size_t fallback_password_len,
    wifi_config_t *wifi_config);

void WifiConnect(const wifi_config_t& wifi_config, bool is_station);

}  // namespace hackvac

#endif  // WIFI_H_
