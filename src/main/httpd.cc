#include "httpd.h"

#include "constants.h"
#include "boot_state.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "event_log.h"
#include "jsmn.h"
#include "mbedtls/md5.h"
#include "mongoose.h"

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

constexpr char kTag[] = "hackvac:httpd";

constexpr mg_str kEmptyJson = MG_MK_STR("{}");

mg_mgr g_mgr;

void MongooseEventHandler(struct mg_connection *nc,
					 int event,
					 void *eventData,
                          void *user_data) {
//  ESP_LOGI(kTag, "Event %d", event);
  if (event == MG_EV_HTTP_REQUEST) {
    http_message* message = static_cast<http_message*>(eventData);
    ESP_LOGI(kTag, "HTTP received: %.*s for %.*s", message->method.len, message->method.p, message->uri.len, message->uri.p);
    mg_send_head(nc, 404, HTML_LEN(resp404_html), "Content-Type: text/html");
    mg_send(nc, HTML_CONTENTS(resp404_html), HTML_LEN(resp404_html));
  }
}

void HandleLedOn(mg_connection* nc, int event, void *ev_data, void* user_data) {
  ESP_LOGI(kTag, "Light on");
  gpio_set_level(BLINK_GPIO, 1);

  mg_send_head(nc, 200, HTML_LEN(index_html), "Content-Type: text/html");
  mg_send(nc, HTML_CONTENTS(index_html), HTML_LEN(index_html));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleLedOff(mg_connection *nc, int event, void *ev_data, void* user_data) {
  ESP_LOGI(kTag, "Light off");
  gpio_set_level(BLINK_GPIO, 0);

  mg_send_head(nc, 200, HTML_LEN(index_html), "Content-Type: text/html");
  mg_send(nc, HTML_CONTENTS(index_html), HTML_LEN(index_html));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleIndex(mg_connection *nc, int event, void *ev_data, void *user_data) {
  ESP_LOGI(kTag, "Index");

  mg_send_head(nc, 200, HTML_LEN(index_html), "Content-Type: text/html");
  mg_send(nc, HTML_CONTENTS(index_html), HTML_LEN(index_html));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleRestart(mg_connection *nc, int event, void *ev_data, void* user_data) {
  esp_restart();
}

// TODO(awong): Timeout net connections? Otherwise the server can be jammed.
//  Look at mg_set_timer.
void HandleEventsStream(mg_connection *nc, int event, void *ev_data, void* user_data) {
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

void HandleBroadcast(mg_connection* nc, int ev, void* ev_data, void* user_data) {
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

void HttpdTask(void *parameters) {
  ESP_LOGI(kTag, "Binding port 80");
  mg_mgr_init(&g_mgr, NULL); // TODO(awong): Move this into its own init.
  mg_connection *c = mg_bind(&g_mgr, ":80", &MongooseEventHandler, nullptr);

  mg_set_protocol_http_websocket(c);
  mg_register_http_endpoint(c, "/$", &HandleIndex, nullptr);
  mg_register_http_endpoint(c, "/led_on$", &HandleLedOn, nullptr);
  mg_register_http_endpoint(c, "/led_off$", &HandleLedOff, nullptr);
  mg_register_http_endpoint(c, "/api/events$", &HandleEventsStream, nullptr);
  mg_register_http_endpoint(c, "/api/restart$", &HandleRestart, nullptr);

  while(1) {
    mg_mgr_poll(&g_mgr, 10000);
  }
}

void HttpdPublishEvent(void* data, size_t len) {
  mg_broadcast(&g_mgr, &HandleBroadcast, data, len);
}

}  // namespace hackvac
