#include "httpd.h"

#include "constants.h"
#include "esp_log.h"
#include "mongoose.h"

namespace hackvac {
namespace {

constexpr char kTag[] = "hackvac:httpd";

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

void MongooseEventHandler(struct mg_connection *nc,
					 int event,
					 void *eventData) {
  ESP_LOGI(kTag, "Event %d", event);
  if (event == MG_EV_HTTP_REQUEST) {
    http_message* message = static_cast<http_message*>(eventData);
    ESP_LOGI(kTag, "HTTP received: %.*s for %.*s", message->method.len, message->method.p, message->uri.len, message->uri.p);
    mg_send_head(nc, 404, sizeof(k404Html), "Content-Type: text/html");
    mg_printf(nc, "%s", k404Html);
  }
}

void HandleLedOn(mg_connection* nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Light on");
  gpio_set_level(BLINK_GPIO, 1);

  mg_send_head(nc, 200, sizeof(kIndexHtml), "Content-Type: text/html");
  mg_printf(nc, "%s", kIndexHtml);
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleLedOff(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Light off");
  gpio_set_level(BLINK_GPIO, 0);

  mg_send_head(nc, 200, sizeof(kIndexHtml), "Content-Type: text/html");
  mg_printf(nc, "%s", kIndexHtml);
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleIndex(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Index");

  mg_send_head(nc, 200, sizeof(kIndexHtml), "Content-Type: text/html");
  mg_printf(nc, "%s", kIndexHtml);
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

void HandleWifiConfig(mg_connection *nc, int event, void *ev_data) {
  ESP_LOGD(kTag, "Write wifi config");
  http_message* hm = static_cast<http_message*>(ev_data);

  if (mg_vcmp(hm->method), "GET") {
    //GetWifiSsid("hi");
    //GetWifiPassword("mom");
  } else if (mg_vcmp(hm->method), "POST") {
    // TODO(awong): Get json.
    SetWifiSsid("hi");
    SetWifiPassword("mom");
  }

  mg_send_head(nc, 200, sizeof(kIndexHtml), "Content-Type: text/html");
  mg_printf(nc, "%s", kIndexHtml);
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
