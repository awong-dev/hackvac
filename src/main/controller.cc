#include "controller.h"

#include <mutex>

#include "esp_cxx/uart.h"
#include "esp_cxx/logging.h"
#include "event_log.h"

namespace {
constexpr char kTag[] = "controller";

constexpr esp_cxx::Uart::Chip kCn105Uart = esp_cxx::Uart::Chip::kUart1;
constexpr esp_cxx::Gpio kCn105TxPin = esp_cxx::Gpio::Pin<18>();
constexpr esp_cxx::Gpio kCn105RxPin = esp_cxx::Gpio::Pin<19>();

constexpr esp_cxx::Uart::Chip kTstatUart = esp_cxx::Uart::Chip::kUart2;
constexpr esp_cxx::Gpio kTstatTxPin = esp_cxx::Gpio::Pin<5>();
constexpr esp_cxx::Gpio kTstatRxPin = esp_cxx::Gpio::Pin<17>();

}  // namespace

namespace hackvac {

const char Controller::kTstatRxTag[] = "T-Rx";
const char Controller::kTstatTxTag[] = "T-Tx";
const char Controller::kHvacRxTag[] = "H-Rx";
const char Controller::kHvacTxTag[] = "H-Tx";

StoredHvacSettings Controller::SharedData::GetStoredHvacSettings() const {
  std::lock_guard<esp_cxx::Mutex> lock_(mutex_);
  return hvac_settings_;
}

void Controller::SharedData::SetStoredHvacSettings(const HvacSettings& hvac_settings) {
  {
    std::lock_guard<esp_cxx::Mutex> lock_(mutex_);
    hvac_settings_ = hvac_settings;
  }
  /* Crashes due to optional.
  ESP_LOGI(kTag, "settings: p:%d m:%d, t:%d, f:%d, v:%d, wv:%d",
           static_cast<int32_t>(hvac_settings.Get<Power>().value()),
           static_cast<int32_t>(hvac_settings.Get<Mode>().value()),
           hvac_settings.GetTargetTemp().value().whole_degree(),
           static_cast<int32_t>(hvac_settings.Get<Fan>().value()),
           static_cast<int32_t>(hvac_settings.Get<Vane>().value()),
           static_cast<int32_t>(hvac_settings.Get<WideVane>().value()));
           */
}

StoredExtendedSettings Controller::SharedData::GetExtendedSettings() const {
  std::lock_guard<esp_cxx::Mutex> lock_(mutex_);
  return extended_settings_;
}

void Controller::SharedData::SetExtendedSettings(const ExtendedSettings& extended_settings) {
  {
    std::lock_guard<esp_cxx::Mutex> lock_(mutex_);
    extended_settings_ = extended_settings;
  }
//  ESP_LOGI(kTag, "extended settings: rt:%d",
//           static_cast<int32_t>(extended_settings.GetRoomTemp().value().whole_degree()));
}

Controller::Controller(esp_cxx::QueueSetEventManager* event_manager)
  : event_manager_(event_manager),
    hvac_control_(event_manager_, kCn105Uart, kCn105TxPin, kCn105RxPin,
                  // TODO(awong): Send status to the controller about once a second.
                  // Sequence seems to be:
                  //    Info: kSettings,
                  //    Info: kExtendedSettings,
                  //    Update: kSettings.
                  //
                  //    Needs periodic task to does an EnqueuePacket() and waits for responses.
                  [this](std::unique_ptr<Cn105Packet> packet) {
                    OnHvacControlPacket(std::move(packet));
                  },
                  [this](std::unique_ptr<Cn105Packet> packet) {
                    packet_logger_->Log(kHvacTxTag, std::move(packet));
                  }),
    thermostat_(event_manager_, kTstatUart, kTstatTxPin, kTstatRxPin,
                [this](std::unique_ptr<Cn105Packet> packet) {
                  OnThermostatPacket(std::move(packet));
                },
                [this](std::unique_ptr<Cn105Packet> packet) {
                  packet_logger_->Log(kTstatTxTag, std::move(packet));
                },
                esp_cxx::Gpio::Pin<23>(),
                esp_cxx::Gpio::Pin<22>()) {
}

Controller::~Controller() {
}

void Controller::Start() {
  hvac_control()->Start();
  thermostat()->Start();
  ScheduleCommand(Command::kConnect);
}

void Controller::SetTemperature(HalfDegreeTemp temp) {
  StoredHvacSettings new_settings = shared_data_.GetStoredHvacSettings();
  new_settings.SetTargetTemp(temp);
  shared_data_.SetStoredHvacSettings(new_settings);

  event_manager_->Run([this]{ ScheduleCommand(Command::kPushSettings); });
}

void Controller::PushSettings(const HvacSettings& settings) {
  shared_data_.SetStoredHvacSettings(settings);
  event_manager_->Run([this]{ ScheduleCommand(Command::kPushSettings); });
}

void Controller::PushExtendedSettings(
    const ExtendedSettings& extended_settings) {
  shared_data_.SetExtendedSettings(extended_settings);
  event_manager_->Run([this]{ ScheduleCommand(Command::kPushExtendedSettings); });
}


void Controller::ScheduleCommand(Command command) {
  command_queue_.push_front(command);
  ExecuteNextCommand();
}

void Controller::ExecuteNextCommand() {
  // A new command can cause this loop to enter before the current command
  // is complete. In this case, just return as command completion will
  // cause this function to execute again.
  if (is_command_oustanding_) {
    return;
  }

  if (command_queue_.empty()) {
    return;
  }
  Command command = command_queue_.front();
  command_queue_.pop_front();

  switch (command) {
    case Command::kConnect:
      hvac_control()->EnqueuePacket(ConnectPacket::Create());
      break;

    case Command::kQuerySettings:
      hvac_control()->EnqueuePacket(InfoPacket::Create(CommandType::kSettings));
      break;

    case Command::kQueryExtendedSettings:
      hvac_control()->EnqueuePacket(InfoPacket::Create(CommandType::kExtendedSettings));
      break;

    case Command::kPushSettings:
      hvac_control()->EnqueuePacket(UpdatePacket::Create(shared_data_.GetStoredHvacSettings()));
      break;

    case Command::kPushExtendedSettings:
      hvac_control()->EnqueuePacket(UpdatePacket::Create(shared_data_.GetExtendedSettings()));
      break;
  }

  command_number_++;
  is_command_oustanding_ = false;
  constexpr int kProtocolTimeoutMs = 20;
  event_manager_->RunDelayed(
      [this, prev_command_number = command_number_] {
        if (prev_command_number == command_number_ &&
            !is_command_oustanding_) {
          ScheduleCommand(Command::kConnect);
        }
      }, kProtocolTimeoutMs);
}

void Controller::OnHvacControlPacket(
    std::unique_ptr<Cn105Packet> hvac_packet) {
  if (is_passthru_) {
    ESP_LOGI(kTag, "hvac_ctl: %d bytes", hvac_packet->packet_size());
    ESP_LOG_BUFFER_HEX_LEVEL(kTag, hvac_packet->raw_bytes(), hvac_packet->packet_size(), ESP_LOG_INFO); 
    thermostat()->EnqueuePacket(std::move(hvac_packet));
    return;
  }
  packet_logger_->Log(kHvacRxTag, std::move(hvac_packet));

  if (hvac_packet->IsJunk()) {
    // Junk is likely either be line-noise or a desyced packet start. Ignore.
    return;
  }

  if (!hvac_packet->IsComplete()) {
    // An incomplete packet means something timed out. Reconnect.
    ScheduleCommand(Command::kConnect);
    return;
  }

  // If a complete packet is found, then consider that to indicate a command
  // has triggered some sort of structurally valid response from the unit and
  // thus the command has not timed out.
  is_command_oustanding_ = true;

  if (!hvac_packet->IsChecksumValid()) {
    // TODO(awong): Increment error count.
    ESP_LOGW(kTag, "Pkt type %d corrupt", static_cast<int>(hvac_packet->type()));
    return;
  }

  switch (hvac_packet->type()) {
    case PacketType::kConnectAck:
    case PacketType::kExtendedConnectAck:
    case PacketType::kUpdateAck:
      // Nothing to do here.
      break;

    case PacketType::kInfoAck: {
      // Nothing to do here.
      InfoAckPacket info_ack(hvac_packet.get());
      if (auto settings = info_ack.settings()) {
        shared_data_.SetStoredHvacSettings(settings.value());
      }

      if (auto extended_settings = info_ack.extended_settings()) {
        shared_data_.SetExtendedSettings(extended_settings.value());
      }
      break;
    }

    default:
      ESP_LOGW(kTag, "Unexpected packet type: %d", static_cast<int>(hvac_packet->type()));
      break;
  }

  ExecuteNextCommand();
}

void Controller::OnThermostatPacket(
    std::unique_ptr<Cn105Packet> thermostat_packet) {
  if (is_passthru_) {
    hvac_control()->EnqueuePacket(std::move(thermostat_packet));
  } else {
    if (!thermostat_packet->IsJunk() &&
        thermostat_packet->IsComplete() &&
        thermostat_packet->IsChecksumValid()) {

      switch (thermostat_packet->type()) {
        case PacketType::kConnect:
          ESP_LOGI(kTag, "Sending ConnectACK");
          thermostat()->EnqueuePacket(ConnectAckPacket::Create());
          break;

        case PacketType::kExtendedConnect:
          ESP_LOGI(kTag, "Sending ExtendedConnectACK");
          // TODO(awong): See if there's a way to understand this packet.
          // Ignoring it for now seems to cause Pac444CN-1 to send 2 attempts
          // and then give up and move on.
          thermostat()->EnqueuePacket(ExtendedConnectAckPacket::Create());
          break;

        case PacketType::kUpdate:
          {
            // TODO(awong): Extract into a process function like CreateInfoAck().
            UpdatePacket update(thermostat_packet.get());

            shared_data_.SetStoredHvacSettings(
                shared_data_.GetStoredHvacSettings().MergeUpdate(
                    update.settings()));

            shared_data_.SetExtendedSettings(
                shared_data_.GetExtendedSettings().MergeUpdate(
                    update.extended_settings()));

            ESP_LOGI(kTag, "Sending UpdateACK");
            thermostat()->EnqueuePacket(UpdateAckPacket::Create());
            break;
          }

        case PacketType::kInfo:
          ESP_LOGI(kTag, "Sending InfoACK");
          thermostat()->EnqueuePacket(
              CreateInfoAck(InfoPacket(thermostat_packet.get())));
          break;

        default:
          break;
      }
    }

    packet_logger_->Log(kTstatRxTag, std::move(thermostat_packet));
  }
}

std::unique_ptr<Cn105Packet> Controller::CreateInfoAck(InfoPacket info) {
  switch (info.type()) {
    case CommandType::kSettings:
      return InfoAckPacket::Create(shared_data_.GetStoredHvacSettings());

    case CommandType::kExtendedSettings:
      return InfoAckPacket::Create(shared_data_.GetExtendedSettings());

      // TODO(awong): Implement.
    case CommandType::kTimers:
    case CommandType::kStatus:
    case CommandType::kEnterStandby:
    default:
      // Unknown packet type. Ack with a status.
      //
      // TODO(awong): Make this actaully a status ack.
      return InfoAckPacket::Create(shared_data_.GetStoredHvacSettings());
  }
}

}  // namespace hackvac
