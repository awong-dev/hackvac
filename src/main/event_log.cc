#include "event_log.h"
#include <atomic>

#include <esp_log.h>
#include <esp_types.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"

#include "httpd.h"

namespace hackvac {
namespace {
std::atomic_int g_listener_count{};

std::atomic<TaskHandle_t> g_publish_task{};
RingbufHandle_t g_log_events;
RingbufHandle_t g_protocol_events;
vprintf_like_t g_original_logger;

// 8kb of log data should be enough for anyone.
constexpr size_t kLogEventsSize = 8192;

// Packets are ~22bytes. This is ~370 packets of history.
constexpr size_t kProtocolEventsSize = 8192;

void LogToOrig(const char* fmt, ...) {
  if (!g_original_logger) return;

  va_list args;
  va_start(args, fmt);
  g_original_logger(fmt, args);
  va_end(args);
}

int LogHook(const char* fmt, va_list argp) {
  int ret_val = g_original_logger(fmt, argp);

  // Print this into the buffer now.
  static constexpr size_t kMaxEntrySize = 512;
  char buf[kMaxEntrySize];
  int len = vsnprintf(buf, sizeof(buf) - 1, fmt, argp);
  buf[sizeof(buf) - 1] = '\0';
  // It'll take at least 20 ticks to log an error to the uart.
  if (xRingbufferSend(g_log_events, &buf[0], len, 20) != pdTRUE) {
    LogToOrig("event dropped");
  }

  return ret_val;
}

void EventLogPublishTask(void* pvParameters) {
  for(;;) {
    // Block until woken
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    while (g_listener_count > 0) {
      size_t item_size;
      void *data = xRingbufferReceive(g_log_events, &item_size, 100000);
      if (data) {
        HttpdPublishEvent(data, item_size);
      }
    }
  }
}

}  // namespace

void IncrementListeners() {
  ++g_listener_count;
  xTaskNotifyGive(g_publish_task);
}

void DecrementListeners() {
  --g_listener_count;
}

void EventLogInit() {
  g_log_events = xRingbufferCreate(kLogEventsSize, RINGBUF_TYPE_NOSPLIT);
  g_protocol_events = xRingbufferCreate(kProtocolEventsSize, RINGBUF_TYPE_NOSPLIT);
  g_original_logger = esp_log_set_vprintf(&LogHook);
  // TODO(awong): Look at xtensa_config.h for stack size.
  TaskHandle_t publish_task = nullptr;
  xTaskCreate(&EventLogPublishTask, "event_log_publish", 4096, NULL, 2, &publish_task);
  g_publish_task = publish_task;
}

}  // namespace hackvac
