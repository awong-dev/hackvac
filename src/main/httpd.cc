#include "httpd.h"

#include "constants.h"
#include "boot_state.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "event_log.h"
#include "jsmn.h"
#include "mbedtls/md5.h"
#include "mongoose.h"
#include "wifi.h"

#include <ctype.h>

// User 1 and 2 are used by the SOCKS code.
#define RECEIVES_EVENT_LOG MG_F_USER_3

#define HTML_DECL(name) \
  extern "C" const uint8_t name##_start[] asm("_binary_" #name "_start"); \
  extern "C" const uint8_t name##_end[] asm("_binary_" #name "_end");
#define HTML_LEN(name) (&name##_end[0] - &name##_start[0] - 1)
#define HTML_CONTENTS(name) (&name##_start[0])

HTML_DECL(resp404_html);
HTML_DECL(index_html);

namespace hackvac {
namespace {

void RestartTask(void* parameters) {
  // Wait one second to restart.
  vTaskDelay(pdMS_TO_TICKS(1000));
  esp_restart();
}

constexpr char kTag[] = "hackvac:httpd";

constexpr mg_str kEmptyJson = MG_MK_STR("{}");

mg_mgr g_mgr;

bool hex_digit(const char input[], unsigned char *val) {
  if (isdigit(input[0])) {
    *val = input[0] - '0';
  } else if ('a' <= input[0] && input[0] <= 'f') {
    *val = 10 + input[0] - 'a';
  } else if ('A' <= input[0] && input[0] <= 'F') {
    *val = 10 + input[0] - 'A';
  } else {
    return false;
  }
  *val <<= 4;
  if (isdigit(input[1])) {
    *val |= input[1] - '0';
  } else if ('a' <= input[1] && input[1] <= 'f') {
    *val |= 10 + input[1] - 'a';
  } else if ('A' <= input[1] && input[1] <= 'F') {
    *val |= 10 + input[0] - 'A';
  } else {
    return false;
  }

  return true;
}

void SendResultJson(mg_connection* nc, int status, const char* msg) {
  size_t len = strlen(msg);
  static constexpr mg_str kSuccessJsonStart = MG_MK_STR("{ 'result': '");
  static constexpr mg_str kSuccessJsonEnd = MG_MK_STR("' }");

  mg_send_head(nc, status, kSuccessJsonStart.len + len + kSuccessJsonEnd.len,
               "Content-Type: application/json");
  mg_send(nc, kSuccessJsonStart.p, kSuccessJsonStart.len);
  mg_send(nc, msg, len);
  mg_send(nc, kSuccessJsonEnd.p, kSuccessJsonEnd.len);
}

mg_str TokenToMgStr(mg_str data, const jsmntok_t& token) {
  mg_str str;
  str.p = data.p + token.start;
  str.len = token.end - token.start;
  return str;
}

void MongooseEventHandler(struct mg_connection *nc,
					 int event,
					 void *eventData) {
//  ESP_LOGI(kTag, "Event %d", event);
  if (event == MG_EV_HTTP_REQUEST) {
    http_message* message = static_cast<http_message*>(eventData);
    ESP_LOGI(kTag, "HTTP received: %.*s for %.*s", message->method.len, message->method.p, message->uri.len, message->uri.p);
    mg_send_head(nc, 404, HTML_LEN(resp404_html), "Content-Type: text/html");
    mg_send(nc, HTML_CONTENTS(resp404_html), HTML_LEN(resp404_html));
  }
}

void HandleLedOn(mg_connection* nc, int event, void *ev_data) {
  ESP_LOGI(kTag, "Light on");
  gpio_set_level(BLINK_GPIO, 1);

  mg_send_head(nc, 200, HTML_LEN(index_html), "Content-Type: text/html");
  mg_send(nc, HTML_CONTENTS(index_html), HTML_LEN(index_html));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleLedOff(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGI(kTag, "Light off");
  gpio_set_level(BLINK_GPIO, 0);

  mg_send_head(nc, 200, HTML_LEN(index_html), "Content-Type: text/html");
  mg_send(nc, HTML_CONTENTS(index_html), HTML_LEN(index_html));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleIndex(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGI(kTag, "Index");

  mg_send_head(nc, 200, HTML_LEN(index_html), "Content-Type: text/html");
  mg_send(nc, HTML_CONTENTS(index_html), HTML_LEN(index_html));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleWifiConfig(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGI(kTag, "Write wifi config");
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
      ESP_LOGI(kTag, "Received %.*s", data.len, data.p);

      static constexpr char kExpectedJson[] =
        "Expected format: { \"ssid\": \"abc\", \"password\": \"123\" }. Use double-quotes!";
      if (tokens[0].type != JSMN_OBJECT ||
          tokens[0].size != 2) {
        ESP_LOGI(kTag, "Expected object with 2 children. Got type %d children %d",
                 tokens[0].type, tokens[0].size);
        return kExpectedJson;
      }

      // All other tokens should be strings and be structured as pairs.
      //
      // JSMN treats 'string' as a JSMN_PRIMITIVE and a "string" as a
      // JSMN_STRING (wtf?). The size field is the number of children
      // so the first should be 1 and the second should be zero.
      for (int i = 0; i < tokens[0].size; i++) {
        int field_start = 1 + 2*i;
        if (tokens[field_start].type != JSMN_STRING
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
      mg_str field = TokenToMgStr(data, field_token);
      if (mg_strcmp(field, kSsidKey) == 0) {
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
      if (mg_strcmp(field, kPasswordKey) == 0) {
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
      SendResultJson(nc, 200, "");
    } else {
      SendResultJson(nc, 400, error);
    }
  }

  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleFirmware(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Got firwamware update");
  bool is_response_sent = false;

  struct OtaContext {
    OtaContext() : has_expected_md5(false), update_partition(nullptr), out_handle(-1) {
      mbedtls_md5_init(&md5_ctx);
      memset(&actual_md5[0], 0xcd, sizeof(actual_md5));
      memset(&expected_md5[0], 0x34, sizeof(expected_md5));
    }

    unsigned char expected_md5[16];
    bool has_expected_md5;
    unsigned char actual_md5[16];
    mbedtls_md5_context md5_ctx;
    const esp_partition_t* update_partition;
    esp_ota_handle_t out_handle;
  };

  switch (event) {
    case MG_EV_HTTP_MULTIPART_REQUEST: {
      ESP_LOGI(kTag, "firmware upload starting");
      nc->user_data = new OtaContext();
      break;
    }
    case MG_EV_HTTP_PART_BEGIN: {
      OtaContext* context =
        static_cast<OtaContext*>(nc->user_data);
      if (!context) return;

      mg_http_multipart_part* multipart =
        static_cast<mg_http_multipart_part*>(ev_data);
      ESP_LOGI(kTag, "Starting multipart: %s", multipart->var_name);
      if (strcmp("firmware", multipart->var_name) == 0) {
        mbedtls_md5_starts(&context->md5_ctx);
        context->update_partition = esp_ota_get_next_update_partition(NULL);
        ESP_ERROR_CHECK(esp_ota_begin(context->update_partition,
                                      OTA_SIZE_UNKNOWN, &context->out_handle));
      }
      break;
    }
    case MG_EV_HTTP_PART_DATA: {
      OtaContext* context =
        static_cast<OtaContext*>(nc->user_data);
      if (!context) return;

      mg_http_multipart_part* multipart =
        static_cast<mg_http_multipart_part*>(ev_data);

      if (strcmp("md5", multipart->var_name) == 0 &&
          multipart->data.len != 0) {
        static constexpr char kExpectsMd5[] =
          "Expecting md5 field to be 32-digit hex string";
        // Reading the md5 checksum. Data should be 32 hex digits.
        if (multipart->data.len != 32) {
          is_response_sent = true;
          mg_send_head(nc, 400, strlen(kExpectsMd5),
                       "Content-Type: text/plain");
          mg_send(nc, kExpectsMd5, strlen(kExpectsMd5));
          goto abort_request;
        }

        ESP_LOGI(kTag, "Read md5");
        context->has_expected_md5 = true;
        for (int i = 0; i < sizeof(context->expected_md5); i++) {
          if (!hex_digit(multipart->data.p + i*2, &context->expected_md5[i])) {
            is_response_sent = true;
            mg_send_head(nc, 400, strlen(kExpectsMd5),
                         "Content-Type: text/plain");
            mg_send(nc, kExpectsMd5, strlen(kExpectsMd5));
            goto abort_request;
          }
        }
      } else if (strcmp("firmware", multipart->var_name) == 0) {
          // This is the firmware blob. Write it! And hash it.
          mbedtls_md5_update(&context->md5_ctx,
                             reinterpret_cast<const unsigned char*>(
                                 multipart->data.p),
                             multipart->data.len); 
          ESP_ERROR_CHECK(esp_ota_write(context->out_handle,
                                        multipart->data.p,
                                        multipart->data.len));
      }
      break;
    }
    case MG_EV_HTTP_PART_END: {
      OtaContext* context = static_cast<OtaContext*>(nc->user_data);
      if (!context) return;
      mg_http_multipart_part* multipart =
          static_cast<mg_http_multipart_part*>(ev_data);
      ESP_LOGI(kTag, "Ending multipart: %s", multipart->var_name);
      if (strcmp("firmware", multipart->var_name) == 0) {
        mbedtls_md5_finish(&context->md5_ctx, &context->actual_md5[0]);
        mbedtls_md5_free(&context->md5_ctx);
      }
      break;
    }
    case MG_EV_HTTP_MULTIPART_REQUEST_END: {
      ESP_LOGI(kTag, "Flashing done");
      OtaContext* context = static_cast<OtaContext*>(nc->user_data);
      if (!context) return;

      int status = 400;
      if (context->has_expected_md5 &&
          memcmp(&context->actual_md5[0], &context->expected_md5[0],
                 sizeof(context->actual_md5)) == 0) {
        status = 200;
        
        ESP_LOGI(kTag, "Setting new OTA to boot.");
        ESP_ERROR_CHECK(esp_ota_end(context->out_handle));
        ESP_ERROR_CHECK(esp_ota_set_boot_partition(context->update_partition));

        // TODO(awong): This should call a shutdown hook.
        ESP_LOGI(kTag, "Prepare to restart system in 1 second!");
        xTaskCreate(&RestartTask, "restart", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
        SetBootState(BootState::FRESH);
      }
      // Yay! All good!
      static constexpr char kFirmwareSuccess[] = "uploaded firmware md5: ";
      mg_send_head(nc, status, strlen(kFirmwareSuccess) + 32,
                   "Content-Type: text/plain");
      mg_send(nc, kFirmwareSuccess, strlen(kFirmwareSuccess));
      for (int i = 0; i < sizeof(context->actual_md5); ++i) {
        mg_printf(nc, "%02x", context->actual_md5[i]);
      }
      is_response_sent = true;
      break;
    }
    default:
      static constexpr char kExpectsFileUpload[] =
        "Expecting http multi-part file upload";
      mg_send_head(nc, 400, strlen(kExpectsFileUpload),
                   "Content-Type: text/plain");
      mg_send(nc, kExpectsFileUpload, strlen(kExpectsFileUpload));
      is_response_sent = true;
      break;
  }
abort_request:

  if (is_response_sent) {
    ESP_LOGD(kTag, "Response sent. Cleaning up.");
    OtaContext* context = static_cast<OtaContext*>(nc->user_data);
    delete context;
    nc->user_data = nullptr;
    nc->flags |= MG_F_SEND_AND_CLOSE;
  }
}

// TODO(awong): Timeout net connections? Otherwise the server can be jammed.
//  Look at mg_set_timer.
void HandleEventsStream(mg_connection *nc, int event, void *ev_data) {
//  ESP_LOGI(kTag, "Requesting event stream %d", event);
  static constexpr char kEventHello[] = "{ 'data': 'Hello' }";
  switch (event) {
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
      ESP_LOGI(kTag, "handhsake done");
      mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, kEventHello,
                              strlen(kEventHello));
      nc->flags |= RECEIVES_EVENT_LOG;
      IncrementListeners();
      break;
    }
    case MG_EV_CLOSE:
      ESP_LOGI(kTag, "closing socket");
      DecrementListeners();
      break;

    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
      ESP_LOGI(kTag, "hs request");
      break;
    case MG_EV_WEBSOCKET_FRAME:
      ESP_LOGI(kTag, "received frame");
      mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, kEventHello,
                              strlen(kEventHello));
      break;
    case MG_EV_WEBSOCKET_CONTROL_FRAME: {
      websocket_message* control_frame = static_cast<websocket_message*>(ev_data);
      ESP_LOGI(kTag, "received control frame. flags %x, data_len %d, data: '%.*s'",
               control_frame->flags & 0xf, control_frame->size, control_frame->size, control_frame->data);
      if ((control_frame->flags & 0xf) ==  WEBSOCKET_OP_PING) {
        mg_send_websocket_frame(nc, WEBSOCKET_OP_PONG, nullptr, 0);
      }
      break;
    }
    default:
      // Ignore these.
      break;
  }
}

void HandleBroadcast(mg_connection* nc, int ev, void* ev_data) {
  if (!(nc->flags & RECEIVES_EVENT_LOG)) {
    return;
  }
  switch (ev) {
    case MG_EV_POLL: {
      // Write a frame here.
      size_t len = strlen(static_cast<const char*>(ev_data));
      mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, ev_data, len);
      break;
    }
    default:
      break;
  }
}

}  // namespace

void HttpdTask(void *pvParameters) {
  ESP_LOGI(kTag, "Binding port 80");
  mg_mgr_init(&g_mgr, NULL); // TODO(awong): Move this into its own init.
  mg_connection *c = mg_bind(&g_mgr, ":80", &MongooseEventHandler);

  mg_set_protocol_http_websocket(c);
  mg_register_http_endpoint(c, "/$", &HandleIndex);
  mg_register_http_endpoint(c, "/led_on$", &HandleLedOn);
  mg_register_http_endpoint(c, "/led_off$", &HandleLedOff);
  mg_register_http_endpoint(c, "/api/firmware$", &HandleFirmware);
  mg_register_http_endpoint(c, "/api/wificonfig$", &HandleWifiConfig);
  mg_register_http_endpoint(c, "/api/events$", &HandleEventsStream);

  while(1) {
    mg_mgr_poll(&g_mgr, 10000);
  }
}

void HttpdPublishEvent(void* data, size_t len) {
  mg_broadcast(&g_mgr, &HandleBroadcast, data, len);
}

}  // namespace hackvac
