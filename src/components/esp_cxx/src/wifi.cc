#include "esp_cxx/wifi.h"

#include <string.h>

#include "esp_cxx/logging.h"
#include "esp_cxx/nvs_handle.h"

#ifndef FAKE_ESP_IDF
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event_loop.h"
#include "esp_wifi.h"
#endif

namespace esp_cxx {

#ifndef FAKE_ESP_IDF
#define fldsiz(name, field) (sizeof(((name *)0)->field))
static_assert(Wifi::kSsidBytes == fldsiz(wifi_config_t, sta.ssid),
              "Ssid field size changed");
static_assert(Wifi::kPasswordBytes == fldsiz(wifi_config_t, sta.password),
              "Password field size changed");
#undef fldsize
#endif  // FAKE_ESP_IDF

namespace {

constexpr char kSsidNvsKey[] = "ssid";
constexpr char kPasswordNvsKey[] = "password";

#ifndef FAKE_ESP_IDF

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
#endif  // FAKE_ESP_IDF

}  // namespace

Wifi::Wifi() = default;

Wifi::~Wifi() {
  Disconnect();
}

std::optional<std::string> Wifi::GetSsid() {
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NvsHandle::Mode::kReadOnly);
  return nvs_wifi_config.GetString(kSsidNvsKey);
}

std::optional<std::string> Wifi::GetPassword() {
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NvsHandle::Mode::kReadOnly);
  return nvs_wifi_config.GetString(kPasswordNvsKey);
}

void Wifi::SetSsid(const std::string& ssid) {
  assert(ssid.size() <= kSsidBytes);
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NvsHandle::Mode::kReadWrite);

  ESP_LOGD(kEspCxxTag, "Writing ssid: %s", ssid.c_str());
  nvs_wifi_config.SetString(kSsidNvsKey, ssid);
}

void Wifi::SetPassword(const std::string& password) {
  assert(password.size() < kPasswordBytes);
  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NvsHandle::Mode::kReadWrite);

  ESP_LOGD(kEspCxxTag, "Writing password: %s", password.c_str());
  nvs_wifi_config.SetString(kPasswordNvsKey, password);
}

bool Wifi::ConnectToAP() {
#ifndef FAKE_ESP_IDF
  wifi_config_t wifi_config = {};

  std::string ssid = GetSsid().value_or(std::string());
  std::string password = GetPassword().value_or(std::string());
  if (ssid.empty() || password.empty() ||
      ssid.size() > sizeof(wifi_config.sta.ssid) ||
      password.size() > sizeof(wifi_config.sta.password)) {
    ESP_LOGE(kEspCxxTag,
             "Stored Ssid or Password has invalid length. Resetting to "
             "empty which may end up here again.");
    SetSsid("");
    SetPassword("");
    return false;
  }

  ESP_LOGW(kEspCxxTag, "Got config ssid: %s password: %s",
           ssid.c_str(), password.c_str());
  strcpy((char*)&wifi_config.sta.ssid[0], ssid.c_str());
  strcpy((char*)&wifi_config.sta.password[0], password.c_str());

  WifiConnect(wifi_config, true);
#endif
  return true;
}

bool Wifi::CreateSetupNetwork(const std::string& setup_ssid,
                              const std::string& setup_password) {
#ifndef FAKE_ESP_IDF
  wifi_config_t wifi_config = {};

  if (setup_ssid.empty() || // Allow empty password.
      setup_ssid.size() > sizeof(wifi_config.ap.ssid) ||
      setup_password.size() > sizeof(wifi_config.ap.password)) {
    ESP_LOGE(kEspCxxTag, "Setup Ssid or Password has invalid length");
    return false;
  }

  // TODO(awong): Don't forget to set country.
  strcpy((char*)&wifi_config.ap.ssid[0], setup_ssid.c_str());
  strcpy((char*)&wifi_config.ap.password[0], setup_password.c_str());

  // Assume null termination always since c_str() is used.
  wifi_config.ap.ssid_len = 0;

  if (!setup_password.empty()) {
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
  }
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.beacon_interval = 100;

  ESP_LOGW(kEspCxxTag, "Creating setup network at ssid: %s password: %s",
           setup_ssid.c_str(), setup_password.c_str());
  WifiConnect(wifi_config, false);
#endif  // FAKE_ESP_IDF
  return true;
}

void Wifi::Disconnect() {
#ifndef FAKE_ESP_IDF
  esp_wifi_stop();
#endif  // FAKE_ESP_IDF
}

}  // namespace esp_cxx
