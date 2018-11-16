#include "controller.h"

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

}  // namespace

namespace hackvac {

Controller::SharedData::SharedData()
  : mux_(portMUX_INITIALIZER_UNLOCKED) {
}

HvacSettings Controller::SharedData::GetHvacSettings() const {
  HvacSettings settings;

  taskENTER_CRITICAL(&mux_);
  settings = hvac_settings_;
  taskEXIT_CRITICAL(&mux_);

  return settings;
}

void Controller::SharedData::SetHvacSettings(const HvacSettings& hvac_settings) {
  taskENTER_CRITICAL(&mux_);
  hvac_settings_ = hvac_settings;
  taskEXIT_CRITICAL(&mux_);
  ESP_LOGI(kTag, "settings: p:%d m:%d, t:%d, f:%d, v:%d, wv:%d",
           static_cast<int32_t>(hvac_settings.power),
           static_cast<int32_t>(hvac_settings.mode),
           31 - static_cast<int32_t>(hvac_settings.target_temp),
           static_cast<int32_t>(hvac_settings.fan),
           static_cast<int32_t>(hvac_settings.vane),
           static_cast<int32_t>(hvac_settings.wide_vane));
}

Controller::Controller()
  : hvac_control_("hvac_ctl", kCn105Uart, kCn105TxPin, kCn105RxPin,
                  [this](std::unique_ptr<Cn105Packet> packet) {
                    this->OnHvacControlPacket(std::move(packet));
                  }),
    thermostat_("tstat", kTstatUart, kTstatTxPin, kTstatRxPin,
                [this](std::unique_ptr<Cn105Packet> packet) {
                  this->OnThermostatPacket(std::move(packet));
                },
                GPIO_NUM_23,
                GPIO_NUM_22) {
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
                           thermostat_packet->raw_bytes_size(), ESP_LOG_INFO);
  if (is_passthru_) {
    hvac_control_.EnqueuePacket(std::move(thermostat_packet));
  } else {
    if (thermostat_packet->IsJunk() ||
        !thermostat_packet->IsComplete() ||
        !thermostat_packet->IsChecksumValid()) {
      ESP_LOGE(kTag, "Bad packet. junk: %d complete %d expected checksum %x actual %x",
               thermostat_packet->IsJunk(),
               thermostat_packet->IsComplete(),
               thermostat_packet->CalculateChecksum(
                   thermostat_packet->raw_bytes(), thermostat_packet->packet_size() - 1),
               thermostat_packet->raw_bytes()[thermostat_packet->packet_size() - 1]);
      return;
    }

    switch (thermostat_packet->type()) {
      case PacketType::kConnect:
        ESP_LOGI(kTag, "Sending ConnectACK");
        thermostat_.EnqueuePacket(ConnectAckPacket::Create());
        break;

      case PacketType::kExtendedConnect:
        ESP_LOGI(kTag, "Sending ExtendedConnectACK");
        // TODO(awong): See if there's a way to understand this packet.
        // Ignoring it for now seems to cause Pac444CN-1 to send 2 attempts
        // and then give up and move on.
        thermostat_.EnqueuePacket(ExtendedConnectAckPacket::Create());
        break;

      case PacketType::kUpdate:
        {
          // TODO(awong): Extract into a process function like CreateInfoAck().
          UpdatePacket update(thermostat_packet.get());
          HvacSettings new_settings = shared_data_.GetHvacSettings();
          update.ApplyUpdate(&new_settings);
          shared_data_.SetHvacSettings(new_settings);
          ESP_LOGI(kTag, "Sending UpdateACK");
          thermostat_.EnqueuePacket(UpdateAckPacket::Create());
          break;
        }

      case PacketType::kInfo:
        ESP_LOGI(kTag, "Sending InfoACK");
        thermostat_.EnqueuePacket(
            CreateInfoAck(InfoPacket(thermostat_packet.get())));
        break;

      default:
        break;
    }
  }
}

std::unique_ptr<Cn105Packet> Controller::CreateInfoAck(InfoPacket info) {
  switch (info.type()) {
    case InfoType::kSettings:
      return InfoAckPacket::Create(shared_data_.GetHvacSettings());

      // TODO(awong): Implement.
    case InfoType::kExtendedSettings:
    case InfoType::kTimers:
    case InfoType::kStatus:
    case InfoType::kEnterStandby:
    default:
      // Unknown packet type. Ack with a status.
      //
      // TODO(awong): Make this actaully a status ack.
      return InfoAckPacket::Create(shared_data_.GetHvacSettings());
  }
}

}  // namespace hackvac
