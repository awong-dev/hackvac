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

StoredHvacSettings Controller::SharedData::GetStoredHvacSettings() const {
  std::lock_guard<esp_cxx::Mutex> lock_(mutex_);
  return hvac_settings_;
}

void Controller::SharedData::SetStoredHvacSettings(const StoredHvacSettings& hvac_settings) {
  {
    std::lock_guard<esp_cxx::Mutex> lock_(mutex_);
    hvac_settings_ = hvac_settings;
  }
  ESP_LOGI(kTag, "settings: p:%d m:%d, t:%d, f:%d, v:%d, wv:%d",
           static_cast<int32_t>(hvac_settings.Get<Power>().value()),
           static_cast<int32_t>(hvac_settings.Get<Mode>().value()),
           hvac_settings.GetTargetTemp().value().whole_degree(),
           static_cast<int32_t>(hvac_settings.Get<Fan>().value()),
           static_cast<int32_t>(hvac_settings.Get<Vane>().value()),
           static_cast<int32_t>(hvac_settings.Get<WideVane>().value()));
}

StoredExtendedSettings Controller::SharedData::GetExtendedSettings() const {
  std::lock_guard<esp_cxx::Mutex> lock_(mutex_);
  return extended_settings_;
}

void Controller::SharedData::SetExtendedSettings(const StoredExtendedSettings& extended_settings) {
  {
    std::lock_guard<esp_cxx::Mutex> lock_(mutex_);
    extended_settings_ = extended_settings;
  }
  ESP_LOGI(kTag, "extended settings: rt:%d",
           static_cast<int32_t>(extended_settings.GetRoomTemp().value().whole_degree()));
}

Controller::Controller()
  : hvac_control_("hvac_ctl", kCn105Uart, kCn105TxPin, kCn105RxPin,
                  // TODO(awong): Send status to the controller about once a second.
                  // Sequence seems to be:
                  //    Info: kSettings,
                  //    Info: kExtendedSettings,
                  //    Update: kSettings.
                  //
                  //    Needs periodic task to does an EnqueuePacket() and waits for responses.
                  [this](std::unique_ptr<Cn105Packet> packet) {
                    this->OnHvacControlPacket(std::move(packet));
                  }),
    thermostat_("tstat", kTstatUart, kTstatTxPin, kTstatRxPin,
                [this](std::unique_ptr<Cn105Packet> packet) {
                  OnThermostatPacket(std::move(packet));
                },
                [this](std::unique_ptr<Cn105Packet> packet) {
                  packet_logger_.Log(std::move(packet));
                },
                esp_cxx::Gpio::Pin<23>(),
                esp_cxx::Gpio::Pin<22>()),
    hvac_packet_rx_queue_(10, sizeof(Cn105Packet*)) {  // TODO(awong): Pull out constant.
}

Controller::~Controller() {
}

void Controller::Start() {
  hvac_control_.Start();
  thermostat_.Start();

  control_task_ = esp_cxx::Task::Create<Controller, &Controller::ControlTaskRunLoop>(
      this, "controller", 4096, 4);
}

void Controller::OnHvacControlPacket(
    std::unique_ptr<Cn105Packet> hvac_packet) {
  if (is_passthru_) {
    ESP_LOGI(kTag, "hvac_ctl: %d bytes", hvac_packet->packet_size());
    ESP_LOG_BUFFER_HEX_LEVEL(kTag, hvac_packet->raw_bytes(), hvac_packet->packet_size(), ESP_LOG_INFO); 
    thermostat_.EnqueuePacket(std::move(hvac_packet));
    return;
  } else {
    // TODO(awong): Decide if HalfDuplexChannel should exclusively expose a
    // RX queue rather than take a callback. That'd make it symmetrical to the
    // HalfDuplexChannel::EnqueuePacket() API.
    Cn105Packet* raw_packet = hvac_packet.release();
    hvac_packet_rx_queue_.Push(&raw_packet, 99999); // TODO(awong): Figure out max delay in MS.
  }
}

void Controller::OnThermostatPacket(
    std::unique_ptr<Cn105Packet> thermostat_packet) {
  if (is_passthru_) {
    hvac_control_.EnqueuePacket(std::move(thermostat_packet));
  } else {
    if (!thermostat_packet->IsJunk() &&
        thermostat_packet->IsComplete() &&
        thermostat_packet->IsChecksumValid()) {

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
            StoredHvacSettings new_settings = shared_data_.GetStoredHvacSettings();
            new_settings.MergeUpdate(update.settings());
            shared_data_.SetStoredHvacSettings(new_settings);
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

    packet_logger_.Log(std::move(thermostat_packet));
  }
}

void Controller::ControlTaskRunLoop() {
  bool is_connected_ = false;
  for (;;) {
    // TODO(awong):
    //   First determine if we have established contact with controller.
    //   If not, then initiaite connect sequence.
    //
    //   If connected, then enter 1s sleep loop. Every 1 s, get info from
    //   controller and publish status updates as necessary. The chatter
    //   sequence is Info:normal, Info:extended, Update:Normal, Update:Extended
    //
    //   On publish, send, and then sleep until update is received from
    //     OnHvacControlPacket. We need a queue. :-/
    //
    //   When waiting, try best to semantically process all packets until expected
    //     one is found, and the continue to chatter sequence.
    //
    //   In the event of a full timeout, update error count. Send channel reset
    //     sequence (crib for PAC-444CN-1). Then restart connect sequence.

    while (!is_connected_) {
      is_connected_ = DoConnect();
    }

    bool at_least_one_suceeded = false;
    // Start chatter sequence by requesting state from HVAC controller.
    std::optional<HvacSettings> settings = QuerySettings();
    at_least_one_suceeded |= !!settings;
    std::optional<ExtendedSettings> hvac_extended_settings = QueryExtendedSettings();
    at_least_one_suceeded |= !!hvac_extended_settings;
    StoredHvacSettings current_settings = shared_data_.GetStoredHvacSettings();
    StoredExtendedSettings current_extended_settings = shared_data_.GetExtendedSettings();

    // TODO(awong): Log the diff from the current state.

    if (!PushSettings(current_settings)) {
      ESP_LOGW(kTag, "Failed settings update");
      is_connected_ = false;
    }

    if (!PushExtendedSettings(current_extended_settings)) {
      ESP_LOGW(kTag, "Failed extended settings update");
      is_connected_ = false;
    }

    // Wait 1 second between chatter bursts.
    esp_cxx::Task::Delay(1000);
  }
}

bool Controller::DoConnect() {
  hvac_control_.EnqueuePacket(ConnectPacket::Create());
  std::unique_ptr<Cn105Packet> connect_ack =
    AwaitPacketOfType(PacketType::kConnectAck);
  // TODO(awong): How should we handle corrupt but still parsable packets?
  return connect_ack && connect_ack->IsChecksumValid();
}

// Queries/Pushes settings over the |hvac_control_| channel.
std::optional<HvacSettings> Controller::QuerySettings() {
  hvac_control_.EnqueuePacket(InfoPacket::Create(CommandType::kSettings));
  std::unique_ptr<Cn105Packet> raw_ack = AwaitPacketOfType(PacketType::kInfoAck);
  InfoAckPacket info_ack(raw_ack.get());

  if (info_ack.IsValid() && info_ack.type() == CommandType::kSettings) {
    return info_ack.settings();
  }

  return {};
}

bool Controller::PushSettings(const StoredHvacSettings& settings) {
//  hvac_control_.EnqueuePacket(UpdatePacket::Create(settings));
  std::unique_ptr<Cn105Packet> raw_ack = AwaitPacketOfType(PacketType::kUpdateAck);
  UpdateAckPacket update_ack(raw_ack.get());

  return update_ack.IsValid() && update_ack.type() == CommandType::kSettings;
}

// Queries/Pushes extended settings over the |hvac_control_| channel.
std::optional<ExtendedSettings> Controller::QueryExtendedSettings() {
  hvac_control_.EnqueuePacket(InfoPacket::Create(CommandType::kExtendedSettings));
  std::unique_ptr<Cn105Packet> raw_ack = AwaitPacketOfType(PacketType::kInfoAck);
  InfoAckPacket info_ack(raw_ack.get());

  // TODO(awong) : Fold this back into the packet.
  if (info_ack.IsValid() && info_ack.type() == CommandType::kExtendedSettings) {
    return info_ack.extended_settings();
  }

  return {};
}

bool Controller::PushExtendedSettings(
    const StoredExtendedSettings& extended_settings) {
  hvac_control_.EnqueuePacket(UpdatePacket::Create(extended_settings));
  std::unique_ptr<Cn105Packet> raw_ack = AwaitPacketOfType(PacketType::kUpdateAck);
  UpdateAckPacket update_ack(raw_ack.get());

  return update_ack.IsValid() && update_ack.type() == CommandType::kExtendedSettings;
}

std::unique_ptr<Cn105Packet> Controller::AwaitPacketOfType(
    PacketType type, int timeout_ms) {
  // TODO(awong): The timeout code is incorrect as multiple receives should
  // use increasily smaller timeouts.

  Cn105Packet* packet = nullptr;

  while (hvac_packet_rx_queue_.Pop(&packet, timeout_ms)) {
    if (packet->type() == type) {
      return std::unique_ptr<Cn105Packet>(packet);
    } else {
      delete packet;
    }
  }
  return {};
}

std::unique_ptr<Cn105Packet> Controller::CreateInfoAck(InfoPacket info) {
  switch (info.type()) {
    case CommandType::kSettings:
      return InfoAckPacket::Create(shared_data_.GetStoredHvacSettings());

      // TODO(awong): Implement.
    case CommandType::kExtendedSettings:
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
