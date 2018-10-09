#include "cpp_entry.h"

#include "boot_state.h"
#include "cn105.h"
#include "constants.h"
#include "event_log.h"
#include "httpd.h"
#include "wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_spi_flash.h"
#include "esp_system.h"

#include "nvs_flash.h"

#include "driver/gpio.h"

static const char *TAG = "hackvac";

void blink_task(void* parameters) {
  static const int BLINK_DELAY_MS = 5000;
  gpio_pad_select_gpio(hackvac::BLINK_GPIO);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(hackvac::BLINK_GPIO, GPIO_MODE_OUTPUT);
  for (;;) {
      /* Blink off (output low) */
      gpio_set_level(hackvac::BLINK_GPIO, 0);
      vTaskDelay(BLINK_DELAY_MS / portTICK_PERIOD_MS);
      /* Blink on (output high) */
      gpio_set_level(hackvac::BLINK_GPIO, 1);
      vTaskDelay(BLINK_DELAY_MS / portTICK_PERIOD_MS);
  }
}

void uptime_task(void* parameters) {
  static const int UPTIME_S = 10;
  int counter = 0;
  for (;;) {
    ESP_LOGI(TAG, "uptime: %ds\n", (counter++) * UPTIME_S);
    vTaskDelay(UPTIME_S * 1000 / portTICK_PERIOD_MS);
  }
}

static void dump_chip_info() {
  /* Print chip information */
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  ESP_LOGI(TAG, "This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
	 chip_info.cores,
	 (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
	 (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

  ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

  ESP_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
	 (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

  fflush(stdout);
}

static void dump_ota_boot_info() {
  const esp_partition_t *configured = esp_ota_get_boot_partition();
  const esp_partition_t *running = esp_ota_get_running_partition();

  if (configured != running) {
    ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
             configured->address, running->address);
    ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
  }
  ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
           running->type, running->subtype, running->address);
}

void cpp_entry() {
  using namespace hackvac;  // TODO(awong): Remove this.
  dump_chip_info();
  dump_ota_boot_info();

  EventLogInit();

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // Silly debug tasks.
  xTaskCreate(&blink_task, "blink_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(&uptime_task, "uptime_task", 4096, NULL, 1, NULL);
  xTaskCreate(&firmware_watchdog_task, "firmware_watchdog_task", 4096, NULL, 1, NULL);

  // Setup Wifi access.
  wifi_config_t wifi_config;
  static constexpr char kFallbackSsid[] = "hackvac_setup";
  static constexpr char kFallbackPassword[] = "cn105rulez";
  bool is_staion = LoadConfigFromNvs(kFallbackSsid, sizeof(kFallbackSsid),
                                     kFallbackPassword, sizeof(kFallbackPassword),
                                     &wifi_config);
  WifiConnect(wifi_config, is_staion);

  // Initialize hackvac controller.
  static hackvac::Controller controller;
  controller.Init();

  // Run webserver.
  xTaskCreate(&HttpdTask, "httpd", XT_STACK_EXTRA_CLIB + 8192, NULL, 2, NULL);

  // TODO(awong): Add idle task hook ot sleep. Use hte ESP32-IDF hooks and don't create a task directly.
}
