#include "router.h"

#include "esp_log.h"

namespace esphttpd {

Router::Router(netconn* conn, RouteDescriptor *routes, size_t num_routes)
  : routes_(routes),
    num_routes_(num_routes),
    conn_(conn) {
}

std::unique_ptr<Route> Router::FindRoute(const char* path, size_t path_len) {
  for (size_t i = 0; i < num_routes_; ++i) {
    const RouteDescriptor& desc = routes_[i];
    if (desc.path_len == path_len &&
        memcmp(desc.path, path, path_len) == 0) {
      return desc.CreateRoute(conn_);
    }
  }
  return nullptr;
}

}  // namespace esphttpd
