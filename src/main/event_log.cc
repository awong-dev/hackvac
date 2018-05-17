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

constexpr char kTag[] = "hackvac:eventlog";

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
  static std::atomic_int reentrancy_guard{};
  if (reentrancy_guard == 0) {
    // This races, but it'll stop an infinite loop regardless.
    reentrancy_guard++;

    // Print this into the buffer now.
    static constexpr size_t kMaxEntrySize = 512;
    char buf[kMaxEntrySize];
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, argp);
    if (len < 0) {
      len = kMaxEntrySize-1;
    }
    buf[sizeof(buf) - 1] = '\0';
    // It'll take at least 20 ticks to log an error to the uart.
    static int event_num = 0;
    event_num++;
    if (xRingbufferSend(g_log_events, &buf[0], len + 1, 40) != pdTRUE) {
      LogToOrig("event dropped %d\n", event_num);
    }
    reentrancy_guard--;
  }

  return ret_val;
}

void EventLogPublishTask(void* pvParameters) {
  for(;;) {
    // Block until woken
    ESP_LOGI(kTag, "Waiting until a listener showed ups");
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    while (g_listener_count > 0) {
      size_t item_size;
      ESP_LOGI(kTag, "Waiting for ring buffer");
      void *data = xRingbufferReceive(g_log_events, &item_size, 100000);
      if (data) {
        ESP_LOGI(kTag, "Publishing data %d, %p, last char %d", item_size, data, static_cast<char*>(data)[item_size]);
        HttpdPublishEvent(data, item_size);
        ESP_LOGI(kTag, "returning data %p", data);
        vRingbufferReturnItem(g_log_events, data);
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
  LogToOrig("^^vvv^^ Logger has been hooked ^^vvv^^\n");
  // TODO(awong): Look at xtensa_config.h for stack size.
  TaskHandle_t publish_task = nullptr;
  // mg_broadcast allocates send buffer on stack. Ensure stack is at least that big.
  static constexpr size_t kMaxMessageSize = 8192;
  xTaskCreate(&EventLogPublishTask, "event_log_publish", XT_STACK_EXTRA_CLIB + kMaxMessageSize, NULL, 2, &publish_task);
  g_publish_task = publish_task;
}

}  // namespace hackvac
