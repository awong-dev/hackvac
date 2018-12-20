#include "esp_cxx/wifi.h"

#include <string.h>

#include "esp_cxx/logging.h"
#include "esp_cxx/nvs_handle.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"

namespace esp_cxx {
namespace {

constexpr char kSsidNvsKey[] = "ssid";
constexpr char kPasswordNvsKey[] = "password";

/* FreeRTOS event group to signal when we are connected*/
// TODO(awong): Why are we bothering with this signal? Can't an atomic be used?
EventGroupHandle_t g_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
constexpr int WIFI_CONNECTED_BIT = BIT0;

esp_err_t EspEventHandler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(kEspCxxTag, "STA_START.");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(kEspCxxTag, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(kEspCxxTag, "station:" MACSTR " join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(kEspCxxTag, "station:" MACSTR "leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(kEspCxxTag, "STA_DISCONNECTED. %d", event->event_info.disconnected.reason);
        esp_wifi_connect();
        xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

}  // namespace


bool LoadConfigFromNvs(
    const char fallback_ssid[], size_t fallback_ssid_len,
    const char fallback_password[], size_t fallback_password_len,
    wifi_config_t *wifi_config) {
  memset(wifi_config, 0, sizeof(wifi_config_t));

  std::string ssid = GetWifiSsid().value_or(std::string());
  std::string password = GetWifiPassword().value_or(std::string());

  assert(ssid.size() <= sizeof(wifi_config->sta.ssid));
  assert(password.size() <= sizeof(wifi_config->sta.password));

  ESP_LOGW(kEspCxxTag, "Got config ssid: %.*s password: %.*s",
           ssid.size(), ssid.data(),
           password.size(), password.data());
  if (!ssid.empty() && !password.empty()) {
    strcpy((char*)&wifi_config->sta.ssid[0], ssid.c_str());
    strcpy((char*)&wifi_config->sta.password[0], password.c_str());
    return true;
  } else {
    // TOOD(awong): Assert on size overage.
    // TODO(awong): Don't forget to set country.
    memcpy(&wifi_config->ap.ssid[0], fallback_ssid, fallback_ssid_len);
    wifi_config->ap.ssid_len = fallback_ssid_len;
    memcpy(&wifi_config->ap.password[0], fallback_password, fallback_password_len);
    if (fallback_password_len > 0) {
      wifi_config->ap.authmode = WIFI_AUTH_WPA2_PSK;
    }
    wifi_config->ap.max_connection = 4;
    wifi_config->ap.beacon_interval = 100;
    return false;
  }
}

void WifiConnect(const wifi_config_t& wifi_config, bool is_station) {
  g_wifi_event_group = xEventGroupCreate();

  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(&EspEventHandler, NULL) );

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  if (is_station) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA,
                                        const_cast<wifi_config_t*>(&wifi_config)));
  } else {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP,
                                        const_cast<wifi_config_t*>(&wifi_config)));
  }
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(kEspCxxTag, "wifi_init finished.");
  ESP_LOGI(kEspCxxTag, "%s SSID:%s password:%s",
           is_station ? "connect to ap" : "created network",
           wifi_config.sta.ssid, wifi_config.sta.password);
}

std::optional<std::string> GetWifiSsid() {
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NVS_READONLY);
  return nvs_wifi_config.GetString(kSsidNvsKey);
}

std::optional<std::string> GetWifiPassword() {
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NVS_READONLY);
  return nvs_wifi_config.GetString(kPasswordNvsKey);
}

void SetWifiSsid(const std::string& ssid) {
  assert(ssid.size() <= kSsidBytes);
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NVS_READWRITE);

  ESP_LOGD(kEspCxxTag, "Writing ssid: %s", ssid.c_str());
  nvs_wifi_config.SetString(kSsidNvsKey, ssid);
}

void SetWifiPassword(const std::string& password) {
  assert(password.size() < kPasswordBytes);
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NVS_READWRITE);

  ESP_LOGD(kEspCxxTag, "Writing password: %s", password.c_str());
  nvs_wifi_config.SetString(kPasswordNvsKey, password);
}

}  // namespace esp_cxx
