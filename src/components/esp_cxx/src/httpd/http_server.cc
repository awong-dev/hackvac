#include "esp_cxx/httpd/http_server.h"

#include "esp_cxx/logging.h"

#include "esp_log.h"

#define HTML_DECL(name) \
  extern "C" const uint8_t name##_start[] asm("_binary_" #name "_start"); \
  extern "C" const uint8_t name##_end[] asm("_binary_" #name "_end");
#define HTML_LEN(name) (&name##_end[0] - &name##_start[0] - 1)
#define HTML_CONTENTS(name) (&name##_start[0])

// TODO(awong): Take this as a string_view.
HTML_DECL(resp404_html);
HTML_DECL(index_html);

namespace esp_cxx {

void HttpServer::Endpoint::OnHttpEventThunk(mg_connection *new_connection, int event,
                                            void *ev_data, void *user_data) {
  Endpoint *endpoint = static_cast<Endpoint*>(user_data);
  bool should_close = false;
  switch (event) {
    case MG_EV_HTTP_REQUEST:
      should_close = true;
      endpoint->OnHttp(
          HttpRequest(static_cast<http_message*>(ev_data)),
          HttpResponse(new_connection));
      break;

    case MG_EV_HTTP_MULTIPART_REQUEST:
      endpoint->OnMultipartStart(
          HttpRequest(static_cast<http_message*>(ev_data)),
          HttpResponse(new_connection));
      break;

    case MG_EV_HTTP_MULTIPART_REQUEST_END:
      should_close = true;
    case MG_EV_HTTP_PART_BEGIN:
    case MG_EV_HTTP_PART_DATA:
    case MG_EV_HTTP_PART_END: {
      HttpMultipart multipart(static_cast<mg_http_multipart_part*>(ev_data),
                              static_cast<HttpMultipart::State>(event));
      endpoint->OnMultipart(multipart, HttpResponse(new_connection));
      break;
    }

    case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
      endpoint->OnWebsocketHandshake(
          HttpRequest(static_cast<http_message*>(ev_data)),
          HttpResponse(new_connection));
      break;

    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
      endpoint->OnWebsocketHandshakeComplete(HttpResponse(new_connection));
      break;

    case MG_EV_WEBSOCKET_CONTROL_FRAME:
    case MG_EV_WEBSOCKET_FRAME:
      endpoint->OnWebsocketFrame(
          WebsocketFrame(static_cast<websocket_message*>(ev_data)),
          WebsocketSender(new_connection));
      break;

    case MG_EV_CLOSE:
      endpoint->OnClose();
      break;

    default:
      should_close = true;
      break;
  }

  if (should_close) {
    new_connection->flags |= MG_F_SEND_AND_CLOSE;
    HttpResponse response(new_connection);
    if (!response.HasSentHeaders()) {
      response.SendError(500);
    }
  }
}

HttpServer::HttpServer(const char* name, const char* port)
  : name_(name),
    port_(port) {
  mg_mgr_init(&event_manager_, this);
  connection_ = mg_bind(&event_manager_, port_, &DefaultHandlerThunk, nullptr);
}

HttpServer::~HttpServer() {
  if (pump_task_) {
    vTaskDelete(pump_task_);
  }
  mg_mgr_free(&event_manager_);
}

void HttpServer::Start() {
  // TODO(awong): Figure out the priority.
  xTaskCreate(&HttpServer::EventPumpThunk, name_, XT_STACK_EXTRA_CLIB, this, 2, &pump_task_);
}

void HttpServer::RegisterEndpoint(const char* path_pattern, Endpoint* endpoint) {
  mg_register_http_endpoint(connection_, path_pattern, &Endpoint::OnHttpEventThunk, endpoint);
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
                                     void *event_data,
                                     void* user_data) {
  if (event == MG_EV_HTTP_REQUEST) {
    http_message* message = static_cast<http_message*>(event_data);
    ESP_LOGI(kEspCxxTag, "HTTP received: %.*s for %.*s", message->method.len, message->method.p, message->uri.len, message->uri.p);
    mg_send_head(nc, 404, HTML_LEN(resp404_html), "Content-Type: text/html");
    mg_send(nc, HTML_CONTENTS(resp404_html), HTML_LEN(resp404_html));
  }
}

}  // namespace esp_cxx
