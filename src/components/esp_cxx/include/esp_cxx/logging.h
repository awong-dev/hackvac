#ifndef ESPCXX_LOGGING_H_
#define ESPCXX_LOGGING_H_

namespace esp_cxx {
constexpr char kEspCxxTag[] = "espcxx";

#ifdef FAKE_ESP_IDF

#include <cstdio>
#define ESP_LOGD(tag, fmt, args...) fprintf(stderr, "D: %s: " fmt, tag, ##args)
#define ESP_LOGI(tag, fmt, args...) fprintf(stderr, "I: %s: " fmt, tag, ##args)
#define ESP_LOGW(tag, fmt, args...) fprintf(stderr, "W: %s: " fmt, tag, ##args)
#define ESP_LOGE(tag, fmt, args...) fprintf(stderr, "E: %s: " fmt, tag, ##args)

#else  // FAKE_ESP_IDF
#include "esp_log.h"
#endif  // FAKE_ESP_IDF

}  // namespace esp_cxx

#endif  // ESPCXX_LOGGING_H_
