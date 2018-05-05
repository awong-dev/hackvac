#include "esphttpd/esphttpd.h"

#include "esphttpd/request_processor.h"
#include "esphttpd/router.h"

#include <memory>

#include "esp_log.h"

#include "http_parser.h"

#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/err.h"

#include "mongoose.h"


static const char TAG[] = "esphttpd";

namespace {

void http_server_netconn_serve(struct netconn *conn,
                               HttpServerConfig* http_server_config) {
  esphttpd::Router router(conn, http_server_config->descriptors,
                         http_server_config->num_routes);
  esphttpd::RequestProcessor request_processor(&router);

  ESP_LOGI(TAG, "starting parse");
  http_parser parser;
  http_parser_init(&parser, HTTP_REQUEST);
  parser.data = &request_processor;

  /* Read the data from the port, blocking if nothing yet there.
   We assume the request (the part we care about) is in one netbuf */

  static constexpr int kDataTimeout = 10000; // 10s w/o data == borked.
  netconn_set_recvtimeout(conn, kDataTimeout);
  err_t err = ERR_OK;
  while (err == ERR_OK && !request_processor.IsFinished()) {
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
  HttpServerConfig* http_server_config =
    static_cast<HttpServerConfig*>(pvParameters);
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
       http_server_netconn_serve(newconn, http_server_config);
       netconn_delete(newconn);
     }
   } while(err == ERR_OK);
   netconn_close(conn);
   netconn_delete(conn);
}

