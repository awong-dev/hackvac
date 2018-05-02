#include <memory>

#include "webserver.h"

#include "constants.h"
#include "led_route.h"
#include "request_processor.h"
#include "router.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "http_parser.h"

#include "nvs_flash.h"

#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/err.h"

static const char TAG[] = "hackvac:web";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

namespace {

class NvsHandle {
  public:
    NvsHandle(const char* name, nvs_open_mode mode) {
      nvs_open(name, mode, &handle_);
    }
    ~NvsHandle() {
      nvs_close(handle_);
    }

    NvsHandle(NvsHandle&& other) : handle_(other.handle_) {
      other.handle_ = 0;
    }

    static NvsHandle OpenWifiConfig(nvs_open_mode mode) {
      return NvsHandle("wifi_config", mode);
    }

    nvs_handle get() const { return handle_; }

  private:
    nvs_handle handle_;

    NvsHandle(NvsHandle&) = delete;
    void operator=(NvsHandle&) = delete;
};

//        esp_err_t err = nvs_set_str(nvs_wifi_config.get(), "ssid", WIFI_SSID );
//        err = nvs_set_str(nvs_wifi_config.get(), "password", WIFI_PASSWORD );

esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "STA_START.");
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR " join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:" MACSTR "leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "STA_DISCONNECTED. %d", event->event_info.disconnected.reason);
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sta() {
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // TODO(awong): Extract to function to create wifi_config_t.
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NVS_READONLY);

    size_t ssid_len;
    size_t password_len;
    esp_err_t err = nvs_get_str(nvs_wifi_config.get(), "ssid", nullptr, &ssid_len);
    err = nvs_get_str(nvs_wifi_config.get(), "password", nullptr, &password_len);
    if (err == ESP_OK ||
        ssid_len > sizeof(wifi_config.sta.ssid) ||
        password_len > sizeof(wifi_config.sta.password)) {
      // TODO(awong): Error out here.
    }
    nvs_get_str(nvs_wifi_config.get(), "ssid", (char*)&wifi_config.sta.ssid[0], &ssid_len);
    nvs_get_str(nvs_wifi_config.get(), "password", (char*)&wifi_config.sta.password[0], &password_len);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             wifi_config.sta.ssid, wifi_config.sta.password);
}

void http_server_netconn_serve(struct netconn *conn) {
  static hackvac::RouteDescriptor descriptors[] = {
    {"/h", 2, &hackvac::LedRoute::CreateRoute},
    {"/l", 2, &hackvac::LedRoute::CreateRoute},
  };

  hackvac::Router router(conn, &descriptors[0], 2);
  hackvac::RequestProcessor request_processor(&router);

  ESP_LOGI(TAG, "starting parse");
  http_parser parser;
  http_parser_init(&parser, HTTP_REQUEST);
  parser.data = &request_processor;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */

  static constexpr int kDataTimeout = 10000; // 10s w/o data == borked.
  netconn_set_recvtimeout(conn, kDataTimeout);
  err_t err = ERR_OK;
  while (err == ERR_OK && !request_processor.IsFinished()) {
    netbuf* inbuf = nullptr;
    err = netconn_recv(conn, &inbuf);
    // Take ownership of inbuf.
    std::unique_ptr<netbuf, decltype(&netbuf_delete)> inbuf_deleter(inbuf, netbuf_delete);

    if (err != ERR_OK) {
      request_processor.OnNetError(err);
      break;
    }

    char *buf = nullptr;
    u16_t buflen = 0;
    err = netbuf_data(inbuf, (void**)&buf, &buflen);
    if (err != ERR_OK) {
      request_processor.OnNetError(err);
      break;
    }
    ESP_LOGI(TAG, "buffer = %.*s\n", buflen, buf);

    size_t bytes_parsed = http_parser_execute(&parser,
        request_processor.parser_settings(), buf, buflen);
    ESP_LOGI(TAG, "bytes read: %zd\n", bytes_parsed);
    if (parser.http_errno) {
      request_processor.OnParseError(static_cast<http_errno>(parser.http_errno));
      ESP_LOGW(TAG, "Failed at byte %zd\n", bytes_parsed);
    }
    // TODO(ajwong): How to handle multiple requests on one socket connection?
    // Do we need to reset the parser at end of 1 message?
  }
  ESP_LOGI(TAG, "connection complete.");

  /* Close the connection (server closes in HTTP) */
  netconn_close(conn);
}

}  // namespace

void http_server_task(void *pvParameters) {
  struct netconn *conn, *newconn;
  err_t err;
  ESP_LOGI(TAG, "http server task started.");
  conn = netconn_new(NETCONN_TCP);
  netconn_bind(conn, NULL, 80);
  ESP_LOGI(TAG, "bound to 80.");
  netconn_listen(conn);
  ESP_LOGI(TAG, "listening.");
  do {
     err = netconn_accept(conn, &newconn);
     if (err == ERR_OK) {
       http_server_netconn_serve(newconn);
       netconn_delete(newconn);
     }
   } while(err == ERR_OK);
   netconn_close(conn);
   netconn_delete(conn);
}

void wifi_connect() {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_init_sta();
}

