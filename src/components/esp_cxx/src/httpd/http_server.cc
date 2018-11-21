#include "esp_cxx/httpd/http_server.h"

#include "esp_log.h"

#define HTML_DECL(name) \
  extern "C" const uint8_t name##_start[] asm("_binary_" #name "_start"); \
  extern "C" const uint8_t name##_end[] asm("_binary_" #name "_end");
#define HTML_LEN(name) (&name##_end[0] - &name##_start[0] - 1)
#define HTML_CONTENTS(name) (&name##_start[0])

// TODO(awong): Take this as a string_view.
HTML_DECL(resp404_html);
HTML_DECL(index_html);

// TODO(awong): Make this configurable.
constexpr char kTag[] = "http";

namespace esp_cxx {

void HttpServer::Endpoint::OnHttpEventThunk(mg_connection *new_connection, int event,
                                            void *ev_data, void *user_data) {
  Endpoint *endpoint = static_cast<Endpoint*>(user_data);
  switch (event) {
    case MG_EV_HTTP_REQUEST:
    case MG_EV_HTTP_MULTIPART_REQUEST: {
      HttpRequest request(static_cast<http_message*>(ev_data));
      endpoint->OnHttp(request, HttpResponse(new_connection));
      return;
    }

    case MG_EV_HTTP_PART_BEGIN:
    case MG_EV_HTTP_PART_DATA:
    case MG_EV_HTTP_PART_END:
    case MG_EV_HTTP_MULTIPART_REQUEST_END: {
      HttpMultipart multipart(static_cast<mg_http_multipart_part*>(ev_data),
                              static_cast<HttpMultipart::State>(event));
      endpoint->OnMultipart(&multipart, HttpResponse(new_connection));
      return;
    }

    default:
      return;
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

void HttpServer::AddEndpoint(const char* path_pattern,
                             void (*handler)(mg_connection*, int event, void* ev_data, void* user_data)) {
  mg_register_http_endpoint(connection_, path_pattern, handler, nullptr);
}

void HttpServer::AddEndpoint(const char* path_pattern, Endpoint* endpoint) {
  mg_register_http_endpoint(connection_, path_pattern, &Endpoint::OnHttpEventThunk, endpoint);
}

void HttpServer::CxxHandlerWrapper(mg_connection* new_connection, int event, void* ev_data,
                                   HttpCallback callback, HttpMultipartCallback multipart_cb) {
  switch (event) {
    case MG_EV_HTTP_REQUEST:
    case MG_EV_HTTP_MULTIPART_REQUEST: {
      HttpRequest request(static_cast<http_message*>(ev_data));
      callback(request, HttpResponse(new_connection));
      return;
    }

    case MG_EV_HTTP_PART_BEGIN:
    case MG_EV_HTTP_PART_DATA:
    case MG_EV_HTTP_PART_END:
    case MG_EV_HTTP_MULTIPART_REQUEST_END: {
      HttpMultipart multipart(static_cast<mg_http_multipart_part*>(ev_data),
                              static_cast<HttpMultipart::State>(event));
      multipart_cb(&multipart, HttpResponse(new_connection));
      return;
    }

    default:
      return;
  }
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
    ESP_LOGI(kTag, "HTTP received: %.*s for %.*s", message->method.len, message->method.p, message->uri.len, message->uri.p);
    mg_send_head(nc, 404, HTML_LEN(resp404_html), "Content-Type: text/html");
    mg_send(nc, HTML_CONTENTS(resp404_html), HTML_LEN(resp404_html));
  }
}

}  // namespace esp_cxx
