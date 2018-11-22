#ifndef ESPCXX_HTTPD_HTTP_SERVER_H_
#define ESPCXX_HTTPD_HTTP_SERVER_H_

#include "mongoose.h"

#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/httpd/http_response.h"

namespace esp_cxx {

class HttpServer {
 public:
  HttpServer(const char* name, const char* port);
  ~HttpServer();

  class Endpoint {
   public:
    virtual ~Endpoint() = default;

    // A plain HTTP request has arrived.
    virtual void OnHttp(HttpRequest request, HttpResponse response) {}

    // The connection has been closed. For whatever reason.
    virtual void OnClose() {}
    
    // TODO(awong): Support chunked http.

    // Multipart lifecycle events.
    // OnMultipartStart - client has requested a multipart upload.
    // OnMultipart - a segment of data from the multipart stream is available.
    //               The filename and varname are available for all related
    //               pieces of a chunk.
    virtual void OnMultipartStart(HttpRequest request, HttpResponse response) {}
    virtual void OnMultipart(HttpMultipart multipart, HttpResponse response) {}

    // Websocket lifecycle events.
    // OnWebsocketHandshake - client has requested a websocket connection.
    // OnWebsocketHandshakeComplete - client has completed handshake.
    // OnWebsocketFrame - websocket frame is received from client. Only called between
    //                    OnWebsocketHandshakeComplete() and OnWebsocketClosed().
    // OnWebsocketClosed - websocket connection closed. Always called once
    //                     after OnWebsocketHandshake().
    virtual void OnWebsocketHandshake(HttpRequest request, HttpResponse response) {}
    virtual void OnWebsocketHandshakeComplete(HttpResponse response) {}
    virtual void OnWebsocketFrame(WebsocketFrame frame, WebsocketSender sender) {}
    // MG_EV_WEBSOCKET_HANDSHAKE_REQUEST = ev_data = html_message.
    // MG_EV_WEBSOCKET_HANDSHAKE_DONE = null ev_data.
    // MG_EV_WEBSOCKET_FRAME = websocket_message
    // MG_EV_WEBSOCKET_CONTROL_FRAME = websocket_message
    // MG_EV_CLOSE = null ev_data

    static void OnHttpEventThunk(mg_connection *nc, int event,
                                 void *ev_data, void *user_data);
  };

  // Starts the server.
  void Start();

  // Adds an Endpoint handler for the given path_pattern.
  void RegisterEndpoint(const char* path_pattern, Endpoint* endpoint);

  // Conveience wrapper for adding simple http endpoint handler using a bare function.
  typedef void (*HttpCallback)(HttpRequest request, HttpResponse response);
  template <HttpCallback handler>
  void RegisterEndpoint(const char* path_pattern) {
    static struct : Endpoint {
      void OnHttp(HttpRequest request, HttpResponse response) {
        // TODO(awong): rename handler to be consistent with multipart_cb here and elsewhere.
        return handler(request, response);
      }
    } endpoint;
    RegisterEndpoint(path_pattern, &endpoint);
  }

 private:
  // Thunk for executing the actual run loop.
  static void EventPumpThunk(void* parameters);

  // Pumps events for the http server.
  void EventPumpRunLoop();

  // Default event handler for the bound port. Run if no other handler
  // intercepts first.
  static void DefaultHandlerThunk(struct mg_connection *nc,
                                  int event,
                                  void *event_data,
                                  void *user_data);

  // Name for pump task.
  const char* name_;

  // Port to connect to. Usually ":80" is good.
  const char* port_;

  // Bound connection to |port_|.
  mg_connection* connection_;

  // Event manager for all connections on this HTTP server.
  mg_mgr event_manager_;

  // Handle of event pump task.
  TaskHandle_t pump_task_ = nullptr;
};

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_HTTP_SERVER_H_
