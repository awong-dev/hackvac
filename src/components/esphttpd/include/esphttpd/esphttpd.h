#ifndef ESPHTTPD_H_
#define ESPHTTPD_H_

#include "router.h"

struct HttpServerConfig {
  esphttpd::RouteDescriptor* descriptors;
  size_t num_routes;
};
void http_server_task(void *pvParameters);
#endif  // ESPHTTPD_H_
