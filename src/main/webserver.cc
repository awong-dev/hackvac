#include <memory>

#include "webserver.h"

#include "constants.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "nvs_flash.h"

#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/err.h"

#include "http_parser.h"

static const char *TAG = "hackvac:web";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

const static char http_404_hdr[] =
    "HTTP/1.1 404 OK\r\nContent-type: text/html\r\n\r\n";
const static char http_404_hml[] = "<!DOCTYPE html>"
      "<html>\n"
      "<head>\n"
      "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      "  <style type=\"text/css\">\n"
      "    html, body, iframe { margin: 0; padding: 0; height: 100%; }\n"
      "    iframe { display: block; width: 100%; border: none; }\n"
      "  </style>\n"
      "<title>404 ESP32</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1>404, from ESP32!</h1>\n"
      "</body>\n"
      "</html>\n";

const static char http_html_hdr[] =
    "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
const static char http_index_hml[] = "<!DOCTYPE html>"
      "<html>\n"
      "<head>\n"
      "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
      "  <style type=\"text/css\">\n"
      "    html, body, iframe { margin: 0; padding: 0; height: 100%; }\n"
      "    iframe { display: block; width: 100%; border: none; }\n"
      "  </style>\n"
      "<title>HELLO ESP32</title>\n"
      "</head>\n"
      "<body>\n"
      "<h1>Hello World, from ESP32!</h1>\n"
      "</body>\n"
      "</html>\n";

namespace {

struct Route {
  int length;
  const char* path;
  bool OnMethodAndPath(http_method method, const char* path, size_t path_len);
  bool OnHeader(const char* field, size_t field_len, const char* value, size_t value_len);
  bool OnBodyData(const char* data, size_t len);
  bool OnComplete();
};

class Router {
  public:
    explicit Router(Route *routes_array)
      : routes_array_(routes_array) {
    }

    Route* FindRoute(const char* url, size_t url_len) {
      return nullptr;
    }

  private:
    Route* routes_array_;
};

class RequestProcessor {
  public:
    enum class State {
      kParsingMethodAndUrl,
      kParsingHeaderField,
      kParsingHeaderValue,
      kParsingBody,
      kDone,
      kError,
    };
    explicit RequestProcessor(Router* router)
      : router_(router) {
        Reset();
    }

    void Reset() {
      state_ = State::kParsingMethodAndUrl;
      current_route_ = nullptr;
      router_ = nullptr;
      url_len_ = 0;
      header_field_len_ = 0;
      header_value_len_ = 0;
    }

    void OnNetError(int err) {
      state_ = State::kError;
      // TODO(awong): send a 500.
    }

    void OnParseError(http_errno err) {
      state_ = State::kError;
      // TODO(awong): send a 500.
    }


    const http_parser_settings* parser_settings() {
      return &parser_settings_;
    }

  private:
    bool PublishMethodAndUrl(http_method method) {
      http_parser_url url_parser;
      http_parser_url_init(&url_parser);

      int err = http_parser_parse_url(url_, url_len_, method == HTTP_CONNECT, &url_parser);
      if (err) {
        ESP_LOGW(TAG, "Could not parse url (%zd) %.*s", url_len_, url_len_, url_);
        // TODO(awong): Setup 500 response here.
        return false;
      }

      // Set the route and publish the parsed url.
      const char* path = url_ + url_parser.field_data[UF_PATH].off;
      size_t path_len = url_parser.field_data[UF_PATH].len;
      current_route_ = router_->FindRoute(path, path_len);
      if (!current_route_) {
        ESP_LOGW(TAG, "No route for %.*s", url_len_, url_);
        // TODO(awong): Setup 500 response here.
        return false;
      }
      current_route_->OnMethodAndPath(method, path, path_len);
      return true;
    }

    bool PublishHeader() {
      if (header_field_len_ == 0) {
        // There is no header. Do nothing.
        return true;
      }

      // In coming from a prior header value, publish down to route.
      if (current_route_ &&
          !current_route_->OnHeader(header_field_, header_field_len_,
            header_value_, header_value_len_)) {
        ESP_LOGD(TAG, "Route rejected header: %.*s%.*s", header_field_len_,
            header_field_, header_value_len_, header_value_);
        return false;
      }
      return true;
    }

    bool PublishBodyData(http_parser* parser, const char* at, size_t length) {
      if (current_route_) {
        return current_route_->OnBodyData(at, length);
      }
      return true;
    }

    bool PublishComplete() {
      if (current_route_) {
        return current_route_->OnComplete();
      }
      return true;
    }

    template <typename T> class AppendStringHelper;
    template <size_t n>
    class AppendStringHelper<char[n]> {
      public:
      static bool Append(char buf[], size_t* len, const char* addition, size_t addition_len) {
        if (*len + addition_len >= n) {
          ESP_LOGW(TAG, "too long: %.*s%.*s", *len, buf, addition_len, addition);
          return false;
        }
        memcpy(buf + *len, addition, addition_len);
        *len += addition_len;
        return true;
      }
    };
    template <typename T>
    static bool AppendString(T& buf, size_t* len, const char* addition, size_t addition_len) {
      return AppendStringHelper<T>::Append(buf, len, addition, addition_len);
    }

    static RequestProcessor* ToSelf(http_parser* parser) {
      return static_cast<RequestProcessor*>(parser->data);
    }

    static int OnMessageBegin(http_parser* parser) {
      ESP_LOGI(TAG, "Message began");
      RequestProcessor* request_processor = static_cast<RequestProcessor*>(parser->data);
      request_processor->Reset();
      return 0;
    }

    static int OnUrl(http_parser* parser, const char* at, size_t length) {
      ESP_LOGI(TAG, "on_url: %.*s", length, at);
      RequestProcessor* request_processor = ToSelf(parser);
      if (!AppendString(request_processor->url_, &request_processor->url_len_, at, length)) {
        return 1;
      }
      return 0;
    }

    static int OnStatus(http_parser* parser, const char* at, size_t length) {
      ESP_LOGW(TAG, "Unexpected Status in request");
      return -1;
    }


    static int OnHeaderField(http_parser* parser, const char* at, size_t length) {
      RequestProcessor* request_processor = ToSelf(parser);
      if (request_processor->state_ != State::kParsingHeaderField) {
        if (request_processor->state_ == State::kParsingMethodAndUrl) {
          // Select route when transitioning out of kParsingMethodAndUrl.
          if (!request_processor->PublishMethodAndUrl(static_cast<http_method>(parser->method))) {
            return 1;
          }
        } else if (request_processor->state_ == State::kParsingHeaderValue) {
          if (!request_processor->PublishHeader()) {
            return 1;
          }
        }

        request_processor->state_ = State::kParsingHeaderField;
        header_field_len_ = 0;
      }

      if (!AppendString(request_processor->header_field_, &request_processor->header_field_len_, at, length)) {
        return 1;
      }

      return 0;
    }

    static int OnHeaderValue(http_parser* parser, const char* at, size_t length) {
      RequestProcessor* request_processor = ToSelf(parser);
      if (request_processor->state_ != State::kParsingHeaderValue) {
        request_processor->state_ = State::kParsingHeaderValue;
        header_value_len_ = 0;
      }
      if (!AppendString(request_processor->header_value_, &request_processor->header_value_len_, at, length)) {
        return 1;
      }
      return 0;
    }

    static int OnHeadersComplete(http_parser* parser) {
      ESP_LOGI(TAG, "headers complete");
      RequestProcessor* request_processor = ToSelf(parser);
      if (!request_processor->PublishHeader()) {
        return 1;
      }
      return 0;
    }

    static int OnBody(http_parser* parser, const char* at, size_t length) {
      RequestProcessor* request_processor = ToSelf(parser);
      request_processor->state_ = State::kParsingBody;
      if (!request_processor->PublishBodyData(parser, at, length)) {
        return 1;
      }
      return 0;
    }

    static int OnMessageComplete(http_parser* parser) {
      ESP_LOGI(TAG, "message complete");
      RequestProcessor* request_processor = ToSelf(parser);
      request_processor->state_ = State::kDone;
      if (!request_processor->PublishComplete()) {
        return 1;
      }
      return 0;
    }

    static int OnChunkHeader(http_parser* parser) {
      ESP_LOGW(TAG, "Unexpected OnChunkHeader");
      return -1;
    }

    static int OnChunkComplete(http_parser* parser) {
      ESP_LOGW(TAG, "Unexpected OnChunkValue");
      return -1;
    }

    State state_;
    Route* current_route_;
    Router* router_;

    static const http_parser_settings parser_settings_;

    // TODO(awong): Thread safety?
    static char url_[2048];
    static size_t url_len_;

    static char header_field_[1024];
    static size_t header_field_len_;

    static char header_value_[2048];
    static size_t header_value_len_;
};

const http_parser_settings RequestProcessor::parser_settings_ = {
  .on_message_begin = &RequestProcessor::OnMessageBegin,
  .on_url = &RequestProcessor::OnUrl,
  .on_status = &RequestProcessor::OnStatus,
  .on_header_field = &RequestProcessor::OnHeaderField,
  .on_header_value = &RequestProcessor::OnHeaderValue,
  .on_headers_complete = &RequestProcessor::OnHeadersComplete,
  .on_body = &RequestProcessor::OnBody,
  .on_message_complete = &RequestProcessor::OnMessageComplete,
  .on_chunk_header = &RequestProcessor::OnChunkHeader,
  .on_chunk_complete = &RequestProcessor::OnChunkComplete,
};

char RequestProcessor::url_[2048];
size_t RequestProcessor::url_len_ = 0;
char RequestProcessor::header_field_[1024];
size_t RequestProcessor::header_field_len_ = 0;
char RequestProcessor::header_value_[2048];
size_t RequestProcessor::header_value_len_ = 0;



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

struct Request {

  char url[256] = {};
  size_t url_len = 0;
  bool is_complete = false;
  static int on_url(http_parser* parser, const char* at, size_t length) {
    ESP_LOGI(TAG, "on_url: %.*s", length, at);
    Request* request = static_cast<Request*>(parser->data);
    if (request->url_len + length > 255) {
      // Fail the parse.
      return 1;
    }
    memcpy(request->url + request->url_len, at, length);
    request->url_len += length;
    return 0;
  }
  static int on_message_complete(http_parser* parser) {
    ESP_LOGI(TAG, "message complete");
    Request* request = static_cast<Request*>(parser->data);
    request->is_complete = true;
    return 0;
  }
};

bool ExecuteCommand(const char* url_path, size_t length) {
  if (length != 2) {
    return false;
  }

  // Switch by path.
  char command = url_path[1];
  ESP_LOGW(TAG, "Command '%c'", command);
  switch (command) {
    case 'h':
      ESP_LOGW(TAG, "Light off");
      gpio_set_level((gpio_num_t)BLINK_GPIO, 0);
      return true;

    case 'l':
      ESP_LOGW(TAG, "Light on");
      gpio_set_level((gpio_num_t)BLINK_GPIO, 1);
      return false;

    default:
      return false;
  }
}

void ProcessRequest(const Request& request, http_method method, struct netconn* conn) {
  http_parser_url parser_url;
  http_parser_url_init(&parser_url);
  int error = http_parser_parse_url(request.url, request.url_len,
      method == HTTP_CONNECT,
      &parser_url);
  if (error) {
      ESP_LOGW(TAG, "Could not parse url (%zd) %.*s\n", request.url_len,
          request.url_len, request.url);
      return;
  }

  // URL parsed. Handle GET if there's a path.
  bool handled = false;
  if (parser_url.field_set & (1 << UF_PATH)) {
    const char* path = request.url + parser_url.field_data[UF_PATH].off;
    size_t path_len = parser_url.field_data[UF_PATH].len;
    if (method == HTTP_GET) {
      if (path_len == 1) {
        handled = true;
        netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);
        netconn_write(conn, http_index_hml, sizeof(http_index_hml)-1, NETCONN_NOCOPY);
      } else if (path_len > 1) {
        handled = ExecuteCommand(path, path_len);
      }
    } else if (method == HTTP_POST) {
      // TODO(awong): This is wrong.
      if (strncmp("/api/wifi", path, path_len)) {
        // Set mode to json expectaiton.
        // Verify content type.
        // Pare data.
        // Set
        NvsHandle nvs_wifi_config = NvsHandle::OpenWifiConfig(NVS_READWRITE);
        // TODO(awong): Handle not getting ESP_OK and don't commit.
        esp_err_t err = nvs_set_str(nvs_wifi_config.get(), "ssid", WIFI_SSID );
        err = nvs_set_str(nvs_wifi_config.get(), "password", WIFI_PASSWORD );
        if (err == ESP_OK) {
          nvs_commit(nvs_wifi_config.get());
        }
      }
    }
  }

  if (!handled) {
    // Return a 404.
    netconn_write(conn, http_404_hdr, sizeof(http_404_hdr)-1, NETCONN_NOCOPY);
    netconn_write(conn, http_404_hml, sizeof(http_404_hml)-1, NETCONN_NOCOPY);
  }
}

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
  RequestProcessor request_processor(nullptr);

  ESP_LOGI(TAG, "starting parse");
  http_parser parser;
  http_parser_init(&parser, HTTP_REQUEST);
  parser.data = &request_processor;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */

  static constexpr int kDataTimeout = 10000; // 10s w/o data == borked.
  netconn_set_recvtimeout(conn, kDataTimeout);
  err_t err = ERR_OK;
  while (err == ERR_OK) {
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

