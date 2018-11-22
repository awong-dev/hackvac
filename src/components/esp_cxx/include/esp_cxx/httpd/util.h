#ifndef ESPCXX_HTTPD_UTIL_H_
#define ESPCXX_HTTPD_UTIL_H_

#include "esp_cxx/cxx17hack.h"

#include "mongoose.h"

namespace esp_cxx {

constexpr decltype(((mg_connection*)0)->flags) kHeaderSentFlag = MG_F_USER_6;
constexpr decltype(((mg_connection*)0)->flags) kLogStreamFlag = MG_F_USER_5;

static inline std::string_view ToStringView(mg_str s) { return {s.p, s.len}; }

}  // namespace esp_cxx

#endif  // ESPCXX_HTTPD_UTIL_H_
