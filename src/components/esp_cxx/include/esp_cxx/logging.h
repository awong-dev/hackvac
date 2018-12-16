#ifndef ESPCXX_LOGGING_H_
#define ESPCXX_LOGGING_H_

namespace esp_cxx {
constexpr char kEspCxxTag[] = "espcxx";

#if MOCK_ESP_IDF

#include <cstdio>
#define ESP_LOGI(tag, fmt, args...) fprintf(stderr, "I: %s: " fmt, tag, ##args)
#define ESP_LOGW(tag, fmt, args...) fprintf(stderr, "W: %s: " fmt, tag, ##args)
#define ESP_LOGE(tag, fmt, args...) fprintf(stderr, "E: %s: " fmt, tag, ##args)

#else
#include "esp_log.h"
#endif

}  // namespace esp_cxx

#endif  // ESPCXX_LOGGING_H_
