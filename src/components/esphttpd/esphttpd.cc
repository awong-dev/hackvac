#include "esphttpd/esphttpd.h"

#include "esphttpd/request_processor.h"
#include "esphttpd/router.h"

#include <memory>

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

#include "mongoose.h"


static const char TAG[] = "esphttpd";

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

void http_server_netconn_serve(struct netconn *conn,
                               HttpServerConfig* http_server_config) {
  esphttpd::Router router(conn, http_server_config->descriptors,
                         http_server_config->num_routes);
  esphttpd::RequestProcessor request_processor(&router);

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

void MongooseEventHandler(struct mg_connection *nc,
					 int event,
					 void *eventData) {
  if (event == MG_EV_HTTP_REQUEST) {
    http_message* message = static_cast<http_message*>(eventData);
    ESP_LOGI(TAG, "HTTP received: %.*s for %.*s", message->method.len, message->method.p, message->uri.len, message->uri.p);
    static constexpr char kResp[] = "Hi mom.";
    mg_send_head(nc, 200, sizeof(kResp), "Content-Type: text/plain");
    mg_printf(nc, "%s\n", kResp);
  }
}

}  // namespace

void mongoose_server_task(void *pvParameters) {
  HttpServerConfig* http_server_config =
    static_cast<HttpServerConfig*>(pvParameters);

  struct mg_mgr mgr;
  mg_mgr_init(&mgr, NULL);
  struct mg_connection *c = mg_bind(&mgr, ":80", &MongooseEventHandler);
  mg_set_protocol_http_websocket(c);
  while(1) {
    mg_mgr_poll(&mgr, 100000);
  }
}

void http_server_task(void *pvParameters) {
  HttpServerConfig* http_server_config =
    static_cast<HttpServerConfig*>(pvParameters);
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
       http_server_netconn_serve(newconn, http_server_config);
       netconn_delete(newconn);
     }
   } while(err == ERR_OK);
   netconn_close(conn);
   netconn_delete(conn);
}

bool LoadConfigFromNvs(
    const char fallback_ssid[], size_t fallback_ssid_len,
    const char fallback_password[], size_t fallback_password_len,
    wifi_config_t *wifi_config) {
  memset(wifi_config, 0, sizeof(wifi_config_t));

  NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NVS_READONLY);
  size_t ssid_len;
  size_t password_len;
  if (nvs_get_str(nvs_wifi_config.get(), "ssid", nullptr, &ssid_len) == ESP_OK &&
      nvs_get_str(nvs_wifi_config.get(), "password", nullptr, &password_len) == ESP_OK &&
      ssid_len <= sizeof(wifi_config->sta.ssid) &&
      password_len <= sizeof(wifi_config->sta.password)) {
    nvs_get_str(nvs_wifi_config.get(), "ssid", (char*)&wifi_config->sta.ssid[0], &ssid_len);
    nvs_get_str(nvs_wifi_config.get(), "password", (char*)&wifi_config->sta.password[0], &password_len);
    return true;
  } else {
    // TOOD(awong): Assert on size overage.
    // TODO(awong): Don't forget to set country.
    memcpy(&wifi_config->ap.ssid[0], fallback_ssid, fallback_ssid_len);
    wifi_config->ap.ssid_len = ssid_len;
    memcpy(&wifi_config->ap.password[0], fallback_password, fallback_password_len);
    if (fallback_password_len > 0) {
      wifi_config->ap.authmode = WIFI_AUTH_WPA2_PSK;
    }
    wifi_config->ap.max_connection = 4;
    wifi_config->ap.beacon_interval = 100;
    return false;
  }
}

void wifi_connect(const wifi_config_t& wifi_config, bool is_station) {
  wifi_event_group = xEventGroupCreate();

  tcpip_adapter_init();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  if (is_station) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA,
                                        const_cast<wifi_config_t*>(&wifi_config)));
  } else {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP,
                                        const_cast<wifi_config_t*>(&wifi_config)));
  }
  ESP_ERROR_CHECK(esp_wifi_start());

  if (is_station) {
    // Run with DHCP server cause there won't be one.
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_STA));
  }

  ESP_LOGI(TAG, "wifi_init finished.");
  ESP_LOGI(TAG, "%s SSID:%s password:%s",
           is_station ? "connect to ap" : "created network",
           wifi_config.sta.ssid, wifi_config.sta.password);
}

