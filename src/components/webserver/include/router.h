#ifndef ROUTER_H_
#define ROUTER_H_

#include <memory>
#include "string.h"

#include "http_parser.h"

struct netconn;

namespace hackvac {

class Route {
 public:
  Route() = default;
  virtual ~Route() = default;

  virtual bool OnMethodAndPath(http_method method, const char* path, size_t path_len) {
    return true;
  }
  virtual bool OnHeader(const char* field, size_t field_len, const char* value, size_t value_len) {
    return true;
  }
  virtual bool OnBodyData(const char* data, size_t len) {
    return true;
  }
  virtual bool OnComplete() {
    return true;
  }
};

class Handler404 : public Route {
 public:
  static std::unique_ptr<Handler404> Create() {
    return std::unique_ptr<Handler404>(new Handler404());
  }

  virtual bool OnComplete() {
    return true;
  }
};

struct RouteDescriptor {
  const char* path;
  int path_len;
  std::unique_ptr<Route>  (*CreateRoute)(netconn*);
};

class Router {
 public:
  explicit Router(netconn* conn, RouteDescriptor *routes, size_t num_routes);

  std::unique_ptr<Route> FindRoute(const char* path, size_t path_len);

 private:
  RouteDescriptor* routes_;
  size_t num_routes_;
  netconn* conn_;
};

}  // namespace hackvac

#endif  // ROUTER_H_
