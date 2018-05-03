#ifndef ESPHTTPD_H_
#define ESPHTTPD_H_

#include "router.h"

#include "esp_wifi.h"

struct HttpServerConfig {
  esphttpd::RouteDescriptor* descriptors;
  size_t num_routes;
};
void http_server_task(void *pvParameters);

// Initializes a |wifi_config| with the given |ssid| and |password|. If
// |is_station| is true, attempts to connect to an access point with the
// given ssid/password. If |is_station| is false, then this creates a
// station with the given ssid/password tha clients can connect to.
//
// ssid_len and password_len are inclusive of null terminator.
void InitWifiConfig(const char ssid[], size_t ssid_len,
                    const char password[], size_t password_len,
                    bool is_station, wifi_config_t *wifi_config);

// Tries to load the ssid/password from NVS. If the keys are not present,
// uses the |fallback_ssid| and |fallback_password| and starts in AP mode.
// This allows creation of a "setup network" to configure the NVS.
//
// Returns true if ssid/password were loaded meaning |wifi_config| is meant
// for STA mode.
bool LoadConfigFromNvs(
    const char fallback_ssid[], size_t fallback_ssid_len,
    const char fallback_password[], size_t fallback_password_len,
    wifi_config_t *wifi_config);

// Prereq: nvs_flash_init() must have been called.
void wifi_connect(const wifi_config_t& wifi_config, bool is_station);

#endif  // ESPHTTPD_H_
