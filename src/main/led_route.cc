#include "led_route.h"

#include "constants.h"
#include "esp_log.h"
#include "lwip/api.h"

namespace hackvac {
namespace {

constexpr char TAG[] = "hackvac:led_route";

constexpr char http_404_hdr[] =
    "HTTP/1.1 404 OK\r\nContent-type: text/html\r\n\r\n";
constexpr char http_404_hml[] = "<!DOCTYPE html>"
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

constexpr char http_html_hdr[] =
    "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
constexpr char http_index_hml[] = "<!DOCTYPE html>"
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

void Send404(netconn* conn) {
  // Return a 404.
  netconn_write(conn, http_404_hdr, sizeof(http_404_hdr)-1, NETCONN_NOCOPY);
  netconn_write(conn, http_404_hml, sizeof(http_404_hml)-1, NETCONN_NOCOPY);
}

void Send200(netconn* conn) {
  // Return a 404.
  netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);
  netconn_write(conn, http_index_hml, sizeof(http_index_hml)-1, NETCONN_NOCOPY);
}

}  // namespace

// static
std::unique_ptr<esphttpd::Route> LedRoute::CreateRoute(netconn* conn) {
  return std::unique_ptr<esphttpd::Route>(new LedRoute(conn));
}

LedRoute::LedRoute(netconn* conn)
  : conn_(conn) {
}

bool LedRoute::OnMethodAndPath(http_method method, const char* path, size_t path_len) {
  if (path_len != 2) {
    return false;
  }

  if (!ExecuteCommand(path[1])) {
    Send200(conn_);
  } else {
    Send404(conn_);
  }

  return true;
}

bool LedRoute::ExecuteCommand(char command) {
  ESP_LOGD(TAG, "Command '%c'", command);
  switch (command) {
    case 'h':
      ESP_LOGD(TAG, "Light off");
      gpio_set_level(BLINK_GPIO, 0);
      return true;

    case 'l':
      ESP_LOGD(TAG, "Light on");
      gpio_set_level(BLINK_GPIO, 1);
      return false;

    default:
      return false;
  }
}

}  // namespace hackvac
