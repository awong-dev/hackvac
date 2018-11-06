#include "cn105.h"

#include "driver/uart.h"
#include "esp_log.h"

namespace {
constexpr char kTag[] = "controller";

constexpr gpio_num_t kPacPower = GPIO_NUM_4;

constexpr uart_port_t kCn105Uart = UART_NUM_1;
constexpr gpio_num_t kCn105TxPin = GPIO_NUM_18;
constexpr gpio_num_t kCn105RxPin = GPIO_NUM_19;
constexpr uart_port_t kTstatUart = UART_NUM_2;
constexpr gpio_num_t kTstatTxPin = GPIO_NUM_5;
constexpr gpio_num_t kTstatRxPin = GPIO_NUM_17;

/*
constexpr char kPacketConnect[] = {
  0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa8
};
*/

}  // namespace

namespace hackvac {

Controller::Controller()
  : hvac_control_("hvac_ctl", kCn105Uart, kCn105TxPin, kCn105RxPin,
                  [this](std::unique_ptr<Cn105Packet> packet) {
                    this->OnHvacControlPacket(std::move(packet));
                  }),
    thermostat_("tstat", kTstatUart, kTstatTxPin, kTstatRxPin,
                [this](std::unique_ptr<Cn105Packet> packet) {
                  this->OnThermostatPacket(std::move(packet));
                }) {
}

Controller::~Controller() = default;

void Controller::Init() {
  gpio_pad_select_gpio(kPacPower);
  gpio_set_direction(kPacPower, GPIO_MODE_OUTPUT);
  gpio_set_level(kPacPower, 1);  // off.

  hvac_control_.Start();
  thermostat_.Start();
}

void Controller::OnHvacControlPacket(
    std::unique_ptr<Cn105Packet> hvac_packet) {
  if (is_passthru_) {
    ESP_LOGI(kTag, "hvac_ctl: %d bytes", hvac_packet->packet_size());
    ESP_LOG_BUFFER_HEX_LEVEL(kTag, hvac_packet->raw_bytes(), hvac_packet->packet_size(), ESP_LOG_INFO); 
    thermostat_.EnqueuePacket(std::move(hvac_packet));
  }
}

void Controller::OnThermostatPacket(
    std::unique_ptr<Cn105Packet> thermostat_packet) {
  ESP_LOGI(kTag, "tstat: %d bytes", thermostat_packet->packet_size());
  ESP_LOG_BUFFER_HEX_LEVEL(kTag, thermostat_packet->raw_bytes(),
                           thermostat_packet->packet_size(), ESP_LOG_INFO); 
  if (is_passthru_) {
    hvac_control_.EnqueuePacket(std::move(thermostat_packet));
  } else {
    // TODO(ajwong): Check for data-link errors.

    if (!thermostat_packet->IsComplete() ||
        !thermostat_packet->IsChecksumValid()) {
      ESP_LOGE(kTag, "Bad packet. complete %d expected checksum %x actual %x",
               thermostat_packet->IsComplete(),
               thermostat_packet->CalculateChecksum(
                   thermostat_packet->raw_bytes(), thermostat_packet->packet_size() - 1),
               thermostat_packet->raw_bytes()[thermostat_packet->packet_size() - 1]);
      // TODO(ajwong): have a log-packet function.
      return;
    }

    switch (thermostat_packet->type()) {
      case PacketType::kConnect:
        ESP_LOGI(kTag, "Sending ConnectACK");
        thermostat_.EnqueuePacket(ConnectAckPacket::Create());
        break;

      case PacketType::kUpdate:
        thermostat_.EnqueuePacket(UpdateAckPacket::Create());
        break;

      case PacketType::kInfo:
        //thermostat_.EnqueuePacket(UpdateAckPacket::Create());
        break;

      default:
        break;
    }
  }
}

}  // namespace hackvac
