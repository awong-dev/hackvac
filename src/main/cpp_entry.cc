#include "cpp_entry.h"

#include "constants.h"
#include "cn105.h"
#include "esphttpd/esphttpd.h"
#include "led_route.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"

#include "nvs_flash.h"

#include "driver/gpio.h"

#ifndef CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY
#error
#endif


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

void cpp_entry() {
  dump_chip_info();

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_config_t wifi_config;
  static constexpr char kFallbackSsid[] = "hackvac_setup";
  static constexpr char kFallbackPassword[] = "cn105rulez";
  bool is_staion = LoadConfigFromNvs(kFallbackSsid, sizeof(kFallbackSsid),
                                     kFallbackPassword, sizeof(kFallbackPassword),
                                     &wifi_config);

  wifi_connect(wifi_config, is_staion);

  static hackvac::Controller controller;
  controller.Init();


//  xTaskCreate(&cn105_control_task, "cn105_control_task", configMINIMAL_STACK_SIZE, NULL, 3, NULL);

  static esphttpd::RouteDescriptor descriptors[] = {
    {"/h", 2, &hackvac::LedRoute::CreateRoute},
    {"/l", 2, &hackvac::LedRoute::CreateRoute},
  };

  HttpServerConfig http_server_config = {
    descriptors,
    2,  // TODO(awong): Use arraysize.
  };
  xTaskCreate(&http_server_task, "http_server", 4096, &http_server_config, 2, NULL);

  // Silly debug tasks.
  xTaskCreate(&blink_task, "blink_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(&uptime_task, "uptime_task", 4096, NULL, 1, NULL);

  // TODO(awong): Add idle task hook ot sleep. Use hte ESP32-IDF hooks and don't create a task directly.
}
