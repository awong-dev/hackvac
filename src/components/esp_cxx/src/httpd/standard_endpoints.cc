#include "esp_cxx/httpd/standard_endpoints.h"

namespace esp_cxx {

void StandardEndpoints::RegisterEndpoints(HttpServer* server) {
  server->RegisterEndpoint<&WifiConfigEndpoint>("/api/wificonfig$");
  server->RegisterEndpoint<&ResetEndpoint>("/api/reset$");
  server->RegisterEndpoint("/api/ota$", ota_endpoint());

  server->EnableWebsockets();
  server->RegisterEndpoint("/api/logz$", log_stream_endpoint());
}

void StandardEndpoints::ResetEndpoint(HttpRequest request, HttpResponse response) {
  if (request.method() == HttpMethod::kGet) {
    esp_restart();
  }
}

}  // namespace esp_cxx

