#ifndef ESPCXX_HTTPD_WIFI_CONFIG_ENDPOINT_H_
#define ESPCXX_HTTPD_WIFI_CONFIG_ENDPOINT_H_

#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/httpd/http_response.h"

namespace esp_cxx {

void WifiConfigEndpoint(HttpRequest request, HttpResponse response);

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_WIFI_CONFIG_ENDPOINT_H_
