#ifndef ESPCXX_HTTPD_OTA_ENDPOINT_H_
#define ESPCXX_HTTPD_OTA_ENDPOINT_H_

#include "esp_cxx/httpd/http_request.h"
#include "esp_cxx/httpd/http_response.h"

namespace esp_cxx {

void OtaEndpoint(const HttpRequest& request, HttpResponse response);
void OtaMultipartEndpoint(HttpMultipart* multipart, HttpResponse response);
// On multipart, we should get these messages:
//   (1) MG_EV_HTTP_MULTIPART_REQUEST -- just headers up to the first Content-Type. No fields. ev_data == http_message.
//   (2) MG_EV_HTTP_PART_BEGIN. ev_data = mg_http_multipart_part. This will have file_name and var_name set for this chunk any nothing else.
//   (3) MG_EV_HTTP_PART_DATA. ev_data = mg_http_multipart_part.
//   (4) MG_EV_HTTP_PART_END. ev_data = mg_http_multipart_part.
//   (5) MG_EV_HTTP_MULTIPART_REQUEST_END. ev_data = mg_http_multipart_part.

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_OTA_ENDPOINT_H_

