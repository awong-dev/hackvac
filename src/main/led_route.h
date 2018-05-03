#ifndef LED_ROUTE_H_
#define LED_ROUTE_H_

#include "router.h"

struct netconn;

namespace hackvac {

class LedRoute : public esphttpd::Route {
 public:
  virtual ~LedRoute() = default;

  static std::unique_ptr<Route> CreateRoute(netconn* conn);

  virtual bool OnMethodAndPath(http_method method, const char* path, size_t path_len);

 private:
  explicit LedRoute(netconn* conn);
  bool ExecuteCommand(char command);

  netconn* conn_;
};

}  // namespace hackvac

#endif  // LED_ROUTE_H_
