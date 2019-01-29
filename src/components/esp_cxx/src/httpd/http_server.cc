#include "esp_cxx/httpd/http_server.h"

#include "esp_cxx/httpd/event_manager.h"
#include "esp_cxx/logging.h"

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
      endpoint->OnWebsocketHandshakeComplete(WebsocketSender(new_connection));
      break;

    case MG_EV_WEBSOCKET_CONTROL_FRAME:
    case MG_EV_WEBSOCKET_FRAME:
      endpoint->OnWebsocketFrame(
          WebsocketFrame(static_cast<websocket_message*>(ev_data)),
          WebsocketSender(new_connection));
      break;

    case MG_EV_CLOSE:
      if (new_connection->flags & MG_F_IS_WEBSOCKET) {
        endpoint->OnWebsocketClosed(WebsocketSender(new_connection));
      }
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

HttpServer::HttpServer(EventManager* event_manager,
                       const char* port,
                       std::string_view resp404_html)
  : resp404_html_(resp404_html),
    event_manager_(event_manager) {
  connection_ = mg_bind(event_manager->underlying_manager(), port, &DefaultHandlerThunk, this);
}

HttpServer::~HttpServer() = default;

void HttpServer::EnableWebsockets() {
  mg_set_protocol_http_websocket(connection_);
}

void HttpServer::RegisterEndpoint(const char* path_pattern, Endpoint* endpoint) {
  mg_register_http_endpoint(connection_, path_pattern, &Endpoint::OnHttpEventThunk, endpoint);
}

void HttpServer::DefaultHandlerThunk(struct mg_connection *nc,
                                     int event,
                                     void *event_data,
                                     void* user_data) {
  HttpServer* self = static_cast<HttpServer*>(user_data);
  switch (event) {
    case MG_EV_HTTP_REQUEST:
    case MG_EV_HTTP_MULTIPART_REQUEST: {
      http_message* message = static_cast<http_message*>(event_data);
      ESP_LOGI(kEspCxxTag, "HTTP received: %.*s for %.*s",
               message->method.len, message->method.p, message->uri.len, message->uri.p);
      if (self->resp404_html_.empty()) {
        mg_http_send_error(nc, 404, nullptr);
      } else {
        mg_send_head(nc, 404, self->resp404_html_.size(),
                     HttpResponse::kContentTypeHtml);
        mg_send(nc, self->resp404_html_.data(), self->resp404_html_.size());
      }
      nc->flags |= MG_F_SEND_AND_CLOSE;
    }
  }
}

}  // namespace esp_cxx
