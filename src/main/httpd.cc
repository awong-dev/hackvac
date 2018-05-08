#include "httpd.h"

#include "constants.h"
#include "esp_log.h"
#include "jsmn.h"
#include "mongoose.h"
#include "wifi.h"

namespace hackvac {
namespace {

constexpr char kTag[] = "hackvac:httpd";

constexpr mg_str kEmptyJson = MG_MK_STR("{}");
constexpr mg_str kErrorStart = MG_MK_STR("{ 'error': '");
constexpr mg_str kErrorEnd = MG_MK_STR("' }");
constexpr char k404Html[] = "<!DOCTYPE html>"
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

constexpr char kIndexHtml[] = "<!DOCTYPE html>"
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

mg_str TokenToMgStr(mg_str data, const jsmntok_t& token) {
  mg_str str;
  str.p = data.p + token.start;
  str.len = token.end - token.start;
  return str;
}

void MongooseEventHandler(struct mg_connection *nc,
					 int event,
					 void *eventData) {
  ESP_LOGI(kTag, "Event %d", event);
  if (event == MG_EV_HTTP_REQUEST) {
    http_message* message = static_cast<http_message*>(eventData);
    ESP_LOGI(kTag, "HTTP received: %.*s for %.*s", message->method.len, message->method.p, message->uri.len, message->uri.p);
    mg_send_head(nc, 404, sizeof(k404Html), "Content-Type: text/html");
    mg_send(nc, k404Html, strlen(k404Html));
  }
}

void HandleLedOn(mg_connection* nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Light on");
  gpio_set_level(BLINK_GPIO, 1);

  mg_send_head(nc, 200, sizeof(kIndexHtml), "Content-Type: text/html");
  mg_send(nc, kIndexHtml, strlen(kIndexHtml));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleLedOff(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Light off");
  gpio_set_level(BLINK_GPIO, 0);

  mg_send_head(nc, 200, sizeof(kIndexHtml), "Content-Type: text/html");
  mg_send(nc, kIndexHtml, strlen(kIndexHtml));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleIndex(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Index");

  mg_send_head(nc, 200, sizeof(kIndexHtml), "Content-Type: text/html");
  mg_send(nc, kIndexHtml, strlen(kIndexHtml));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleWifiConfig(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Write wifi config");
  http_message* hm = static_cast<http_message*>(ev_data);

  struct WifiConfig {
    // Returns nullptr on success. Otherwise is error message.
    static const char* HandlePost(mg_str data) {
      // { 'ssid': 'blah', 'password', 'blee' }
      // Total of 5 tokens. Object, and 4 strings.
      static constexpr unsigned int kMaxTokens = 5;
      jsmntok_t tokens[kMaxTokens];
      jsmn_parser json_parser;
      jsmn_init(&json_parser);
      int num_tokens = jsmn_parse(&json_parser, data.p, data.len, &tokens[0],
                                  kMaxTokens);
      if (num_tokens < 0) {
        return "Invalid JSON";
      }
      ESP_LOGI(kTag, "Num tokens found %d", num_tokens);

      static constexpr char kExpectedJson[] =
        "Expected format: { 'ssid': 'abc', 'password': '123' }";
      if (tokens[0].type != JSMN_OBJECT ||
          tokens[0].size != 2) {
        ESP_LOGI(kTag, "Expected object with 2 children. Got type %d children %d",
                 tokens[0].type, tokens[0].size);
        return kExpectedJson;
      }

      // All other tokens should be strings and be structured as pairs.
      for (int i = 0; i < tokens[0].size; i++) {
        int field_start = 1 + 2*i;
        if (tokens[field_start].type != JSMN_STRING ||
            tokens[field_start].size != 1 ||
            tokens[field_start + 1].type != JSMN_STRING ||
            tokens[field_start + 1].size != 0) {
          ESP_LOGW(kTag, "Error at field %d", i);
          return kExpectedJson;
        }
      }

      if (!HandleEntry(data, tokens[1], tokens[2])) {
        return "Unable ot process first field";
      }
      if (!HandleEntry(data, tokens[3], tokens[4])) {
        return "Unable ot process second field";
      }
      return nullptr;
    }

    static bool HandleEntry(mg_str data, jsmntok_t field_token,
                            jsmntok_t value_token) {
      static constexpr mg_str kSsidKey = MG_MK_STR("ssid");
      if (mg_strcmp(TokenToMgStr(data, field_token), kSsidKey) == 0) {
        mg_str value = TokenToMgStr(data, value_token);
        char ssid[kSsidBytes];
        if (value.len > sizeof(ssid) - 1) {
          return false;
        }
        memcpy(ssid, value.p, value.len);
        ssid[value.len] = '\0';
        SetWifiSsid(ssid);
      }

      static constexpr mg_str kPasswordKey = MG_MK_STR("password");
      if (mg_strcmp(TokenToMgStr(data, field_token), kPasswordKey) == 0) {
        mg_str value = TokenToMgStr(data, value_token);
        char password[kPasswordBytes];
        if (value.len > sizeof(password) - 1) {
          return false;
        }
        memcpy(password, value.p, value.len);
        password[value.len] = '\0';
        SetWifiPassword(password);
      }

      return true;
    }
  };

  if (mg_vcmp(&hm->method, "GET") == 0) {
    static constexpr mg_str kConfigStart = MG_MK_STR("{ 'ssid': '");
    static constexpr mg_str kConfigMid = MG_MK_STR("', 'password': '");
    static constexpr mg_str kConfigEnd = MG_MK_STR("' }");
    char ssid[kSsidBytes];
    size_t ssid_len = sizeof(ssid);
    char password[kPasswordBytes];
    size_t password_len = sizeof(password);
    if (!GetWifiSsid(&ssid[0], &ssid_len)) {
      constexpr char kNotSet[] = "(not set)";
      strcpy(ssid, kNotSet);
      ssid_len = strlen(kNotSet);
    } else {
      ssid_len--;  // null terminator.
    }
    if (!GetWifiPassword(&password[0], &password_len)) {
      constexpr char kNotSet[] = "(not set)";
      strcpy(password, kNotSet);
      password_len = strlen(kNotSet);
    } else {
      password_len--;  // null terminator.
    }
    mg_send_head(nc, 200,
                 kConfigStart.len + ssid_len + kConfigMid.len +
                     password_len + kConfigEnd.len,
                 "Content-Type: application/json");
    mg_send(nc, kConfigStart.p, kConfigStart.len);
    mg_send(nc, ssid, ssid_len);
    mg_send(nc, kConfigMid.p, kConfigMid.len);
    mg_send(nc, password, password_len);
    mg_send(nc, kConfigEnd.p, kConfigEnd.len);
  } else if (mg_vcmp(&hm->method, "POST") == 0) {
    const char* error = WifiConfig::HandlePost(hm->body);
    if (error == nullptr) {
      mg_send_head(nc, 200, kEmptyJson.len, "Content-Type: application/json");
      mg_send(nc, kEmptyJson.p, kEmptyJson.len);
    } else {
      size_t len = strlen(error);
      mg_send_head(nc, 400, kErrorStart.len + len + kErrorEnd.len,
                   "Content-Type: application/json");
      mg_send(nc, kErrorStart.p, kErrorStart.len);
      mg_send(nc, error, len);
      mg_send(nc, kErrorEnd.p, kErrorEnd.len);
    }
  }

  nc->flags |= MG_F_SEND_AND_CLOSE;
}

}  // namespace

void HttpdTask(void *pvParameters) {
  mg_mgr mgr;
  mg_mgr_init(&mgr, NULL);
  mg_connection *c = mg_bind(&mgr, ":80", &MongooseEventHandler);

  mg_set_protocol_http_websocket(c);
  mg_register_http_endpoint(c, "/led_on", &HandleLedOn);
  mg_register_http_endpoint(c, "/led_off", &HandleLedOff);
  mg_register_http_endpoint(c, "/", &HandleIndex);
  mg_register_http_endpoint(c, "/api/wificonfig", &HandleWifiConfig);

  while(1) {
    mg_mgr_poll(&mgr, 10000);
  }
}

}  // namespace hackvac
