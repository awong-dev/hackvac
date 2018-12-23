#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := hackvac
COMPONENTS ?=  \
  app_trace app_update app_flash bootloader bootloader_support console cxx driver ethernet freertos heap \
  esp32 esp_event esp_ringbuf esptool_py log micro-ecc mdns newlib nvs_flash partition_table \
  pthread smartconfig smartconfig_ack \
  soc spi_flash tcp_transport, tcpip_adapter ulp vfs wpa_supplicant xtensa-debug-module \
  esp_cxx mbedtls json jsmn main mongoose lwip

include $(IDF_PATH)/make/project.mk
