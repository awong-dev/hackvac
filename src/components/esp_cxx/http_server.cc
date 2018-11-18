#include "esp_cxx/httpd/http_server.h"

#include "esp_log.h"

#define HTML_DECL(name) \
  extern "C" const uint8_t name##_start[] asm("_binary_" #name "_start"); \
  extern "C" const uint8_t name##_end[] asm("_binary_" #name "_end");
#define HTML_LEN(name) (&name##_end[0] - &name##_start[0] - 1)
#define HTML_CONTENTS(name) (&name##_start[0])

HTML_DECL(resp404_html);
HTML_DECL(index_html);

// TODO(awong): Make this configurable.
constexpr char kTag[] = "http";

namespace esp_cxx {
HttpServer::HttpServer(const char* name, const char* port)
  : name_(name),
    port_(port) {
}

HttpServer::~HttpServer() {
  mg_mgr_free(&event_manager_);
  vTaskDelete(pump_task_);
}

void HttpServer::Start() {
  mg_mgr_init(&event_manager_, this);
  connection_ = mg_bind(&event_manager_, port_, &DefaultHandlerThunk);

  // TODO(awong): Figure out the priority.
  xTaskCreate(&HttpServer::EventPumpThunk, name_, XT_STACK_EXTRA_CLIB, this, 2, &pump_task_);
}

void HttpServer::EventPumpThunk(void* parameters) {
  static_cast<HttpServer*>(parameters)->EventPumpRunLoop();
}

void HttpServer::EventPumpRunLoop() {
  for(;;) {
    mg_mgr_poll(&event_manager_, 10000);
  }
}

void HttpServer::DefaultHandlerThunk(struct mg_connection *nc,
                                     int event,
                                     void *eventData) {
  if (event == MG_EV_HTTP_REQUEST) {
    http_message* message = static_cast<http_message*>(eventData);
    ESP_LOGI(kTag, "HTTP received: %.*s for %.*s", message->method.len, message->method.p, message->uri.len, message->uri.p);
    mg_send_head(nc, 404, HTML_LEN(resp404_html), "Content-Type: text/html");
    mg_send(nc, HTML_CONTENTS(resp404_html), HTML_LEN(resp404_html));
  }
}

}  // namespace esp_cxx
