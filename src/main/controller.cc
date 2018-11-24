#include "controller.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "event_log.h"

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

HvacSettings Controller::SharedData::GetHvacSettings() const {
  esp_cxx::AutoMutex lock_(&mutex_);
  return hvac_settings_;
}

void Controller::SharedData::SetHvacSettings(const HvacSettings& hvac_settings) {
  {
    esp_cxx::AutoMutex lock_(&mutex_);
    hvac_settings_ = hvac_settings;
  }
  ESP_LOGI(kTag, "settings: p:%d m:%d, t:%d, f:%d, v:%d, wv:%d",
           static_cast<int32_t>(hvac_settings.power),
           static_cast<int32_t>(hvac_settings.mode),
           31 - static_cast<int32_t>(hvac_settings.target_temp),
           static_cast<int32_t>(hvac_settings.fan),
           static_cast<int32_t>(hvac_settings.vane),
           static_cast<int32_t>(hvac_settings.wide_vane));
}

ExtendedSettings Controller::SharedData::GetExtendedSettings() const {
  esp_cxx::AutoMutex lock_(&mutex_);
  return extended_settings_;
}

void Controller::SharedData::SetExtendedSettings(const ExtendedSettings& extended_settings) {
  {
    esp_cxx::AutoMutex lock_(&mutex_);
    extended_settings_ = extended_settings;
  }
  ESP_LOGI(kTag, "extended settings: rt:%d",
           static_cast<int32_t>(extended_settings.room_temp));
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
                GPIO_NUM_23,
                GPIO_NUM_22),
    hvac_packet_rx_queue_(xQueueCreate(10, sizeof(Cn105Packet*))) {  // TODO(awong): Pull out constant.
}

Controller::~Controller() {
  if (control_task_) {
    vTaskDelete(control_task_);
  }
  vQueueDelete(hvac_packet_rx_queue_);
}

void Controller::Start() {
  gpio_pad_select_gpio(kPacPower);
  gpio_set_direction(kPacPower, GPIO_MODE_OUTPUT);
  gpio_set_level(kPacPower, 1);  // off.

  hvac_control_.Start();
  thermostat_.Start();

  xTaskCreate(&Controller::ControlTaskThunk, "controller", 4096, this, 4, &control_task_);
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
    xQueueSendToBack(hvac_packet_rx_queue_, &raw_packet,
                     static_cast<portTickType>(portMAX_DELAY));
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

    packet_logger_.Log(std::move(thermostat_packet));
  }
}

void Controller::ControlTaskThunk(void *parameters) {
  static_cast<Controller*>(parameters)->ControlTaskRunloop();
}

void Controller::ControlTaskRunloop() {
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
    //
    // Wait forever.
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

    while (!is_connected_) {
      is_connected_ = DoConnect();
    }

    bool at_least_one_suceeded = false;
    // Start chatter sequence by requesting state from HVAC controller.
    std::optional<HvacSettings> settings = QuerySettings();
    at_least_one_suceeded |= !!settings;
    std::optional<ExtendedSettings> hvac_extended_settings = QueryExtendedSettings();
    at_least_one_suceeded |= !!hvac_extended_settings;
    HvacSettings current_settings = shared_data_.GetHvacSettings();
    ExtendedSettings current_extended_settings = shared_data_.GetExtendedSettings();

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
    vTaskDelay(1000 / portTICK_PERIOD_MS);
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
  return {};
}

bool Controller::PushSettings(const HvacSettings& settings) {
  return false;
}

// Queries/Pushes extended settings over the |hvac_control_| channel.
std::optional<ExtendedSettings> Controller::QueryExtendedSettings() {
  return {};
}

bool Controller::PushExtendedSettings(
    const ExtendedSettings& extended_settings) {
  return false;
}

std::unique_ptr<Cn105Packet> Controller::AwaitPacketOfType(
    PacketType type, int timeout_ms) {
  // TODO(awong): The timeout code is incorrect as multiple receives should
  // use increasily smaller timeouts.

  Cn105Packet* packet = nullptr;

  while (xQueueReceive(hvac_packet_rx_queue_, &packet, timeout_ms) == pdTRUE) {
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
