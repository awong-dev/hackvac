#include "cpp_entry.h"

#include "controller.h"
#include "event_log.h"

#ifndef FAKE_ESP_IDF
#include "esp_ota_ops.h"
#include "esp_spi_flash.h"
#include "esp_system.h"
#include "nvs_flash.h"
#endif

#include "esp_cxx/gpio.h"
#include "esp_cxx/httpd/http_server.h"
#include "esp_cxx/httpd/standard_endpoints.h"
#include "esp_cxx/logging.h"
#include "esp_cxx/ota.h"
#include "esp_cxx/task.h"
#include "esp_cxx/wifi.h"

#define HTML_DECL(name) \
  extern "C" const uint8_t name##_start[] asm("_binary_" #name "_start"); \
  extern "C" const uint8_t name##_end[] asm("_binary_" #name "_end");
#define HTML_LEN(name) (&name##_end[0] - &name##_start[0] - 1)
#define HTML_CONTENTS(name) (&name##_start[0])

HTML_DECL(resp404_html);
HTML_DECL(index_html);

constexpr esp_cxx::Gpio kBlinkGpio = esp_cxx::Gpio::Pin<2>();

static const char *kTag = "hackvac";

void blink_task_func(void* parameters) {
  static const int BLINK_DELAY_MS = 5000;
//  gpio_pad_select_gpio(hackvac::BLINK_GPIO);
  /* Set the GPIO as a push/pull output */
//  gpio_set_direction(hackvac::BLINK_GPIO, GPIO_MODE_OUTPUT);
  for (;;) {
      /* Blink off (output low) */
      kBlinkGpio.Set(false);
      esp_cxx::Task::Delay(BLINK_DELAY_MS);
      /* Blink on (output high) */
      kBlinkGpio.Set(true);
      esp_cxx::Task::Delay(BLINK_DELAY_MS);
  }
}

void uptime_task_func(void* parameters) {
  static constexpr int kUpdatePublishSec = 1;
  int counter = 0;
  for (;;) {
    ESP_LOGI(kTag, "uptime: %ds\n", (counter++) * kUpdatePublishSec);
    esp_cxx::Task::Delay(kUpdatePublishSec * 1000);
  }
}

static void dump_chip_info() {
#ifndef FAKE_ESP_IDF
  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ESP_LOGI(kTag, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
	 chip_info.cores,
	 (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
	 (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  ESP_LOGI(kTag, "silicon revision %d, ", chip_info.revision);

  ESP_LOGI(kTag, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
	 (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  fflush(stdout);
#endif
}

static void dump_ota_boot_info() {
#ifndef FAKE_ESP_IDF
  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();

  if (configured != running) {
    ESP_LOGW(kTag, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
             configured->address, running->address);
    ESP_LOGW(kTag, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
  }
  ESP_LOGI(kTag, "Running partition type %d subtype %d (offset 0x%08x)",
           running->type, running->subtype, running->address);
#endif
}

void cpp_entry() {
  using namespace hackvac;  // TODO(awong): Remove this.
  dump_chip_info();
  dump_ota_boot_info();

  EventLogInit();

  // Initialize NVS
#ifndef FAKE_ESP_IDF
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
#endif

  // Silly debug tasks.
  esp_cxx::Task blink_task(&blink_task_func, nullptr, "blink_task");
  esp_cxx::Task uptime_task(&uptime_task_func, nullptr, "uptime_task");
  esp_cxx::RunOtaWatchdog();

  // Setup Wifi access.
  // TODO(awong): move all this into a wifi object.
  static constexpr char kFallbackSsid[] = "hackvac_setup";
  static constexpr char kFallbackPassword[] = "cn105rulez";
  esp_cxx::Wifi wifi;
  if (!wifi.ConnectToAP() && !wifi.CreateSetupNetwork(kFallbackSsid, kFallbackPassword)) {
    ESP_LOGE(kTag, "Failed to connect to AP OR create a Setup Network.");
  }

  // Initialize hackvac controller.
  static hackvac::Controller controller;
  //controller.Start();

  // Run webserver.
  std::string_view index_html(
      reinterpret_cast<const char*>(HTML_CONTENTS(index_html)),
      HTML_LEN(index_html));
  std::string_view resp404_html(
      reinterpret_cast<const char*>(HTML_CONTENTS(resp404_html)),
      HTML_LEN(resp404_html));
  esp_cxx::HttpServer http_server("httpd", ":8080", resp404_html);
  esp_cxx::StandardEndpoints standard_endpoints(index_html);
  standard_endpoints.RegisterEndpoints(&http_server);
  http_server.Start();

  // TODO(awong): Add idle task hook ot sleep. Use hte ESP32-IDF hooks and don't create a task directly.
  for (;;) {
    sleep(1000);
  }
}
