#include "event_log.h"

#include <atomic>
#include <string.h>

#include <esp_log.h>
#include <esp_types.h>
#include <cJSON.h>

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
  static std::atomic_uint event_num{};
  unsigned int cur_event_num = ++event_num;

  int ret_val = g_original_logger(fmt, argp);
  // TODO(awong): The early-out logic here is unclear. Rethink how this works.
  if (g_listener_count == 0) {
    return ret_val;
  }

  // Render this into a string now.
  // 512 is send buffer size.
  // 9 is max digits for int32.
  // 17 bytes of overhead for {"n":, "m":""}
  // 1 byte for null terminator.
  // 10 more bytes, incase I miscount.
  static constexpr size_t kMaxEntrySize = 512;
  static constexpr size_t kMaxMsgSize = kMaxEntrySize - 9 - 17 - 1 - 5;
  char buf[kMaxEntrySize];
  int size_left = kMaxMsgSize;
  size_left -= snprintf(buf, size_left, "%u: ", cur_event_num);
  vsnprintf(buf, size_left, fmt, argp);

  // Render into JSON.
  cJSON *root = cJSON_CreateObject();
  if (root &&
      cJSON_AddNumberToObject(root, "n", cur_event_num) &&
      cJSON_AddStringToObject(root, "m", buf) &&
      cJSON_PrintPreallocated(root, buf, sizeof(buf), false) == 1) {
    // At 115200 baud, 1-byte takes 1/(1115200/8) * 1000 * 1000 = 69.4ns.
    // Thus a 512 byte buffer takes minimal 35.6ms. Round to 40ms and give 2x
    // for contention with 1 other write.
    if (xRingbufferSend(g_log_events, &buf[0], strlen(buf) + 1, pdMS_TO_TICKS(80)) != pdTRUE) {
// TODO(ajwong): Figure out how to handle dropped events.
//      LogToOrig("event dropped %u\n", cur_event_num);;
    }
  } else {
    LogToOrig("Could not render json %u\n", cur_event_num);
  }

  cJSON_Delete(root);
  return ret_val;
}

void EventLogPublishTask(void* pvParameters) {
  for(;;) {
    // Block until woken
    LogToOrig("%s: Waiting until a listener to show up\n", kTag);
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
    while (g_listener_count > 0) {
      size_t item_size;
      void *data = xRingbufferReceive(g_log_events, &item_size, 100000);
      if (data) {
        HttpdPublishEvent(data, item_size);
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
