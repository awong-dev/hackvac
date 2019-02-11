#include "../controller.h"

#include "esp_cxx/event_manager.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using testing::AllOf;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Mock;
using testing::NotNull;
using testing::Property;
using testing::_;

namespace hackvac {
class MockHalfDuplexChannel : public HalfDuplexChannel {
 public:
  MockHalfDuplexChannel()
    : HalfDuplexChannel(nullptr, esp_cxx::Uart::Chip::kInvalid, {}, {}, {}) {}
  MOCK_METHOD0(Start, void());
  MOCK_METHOD1(EnqueuePacket, void(std::unique_ptr<Cn105Packet>));
};

class FakeController : public Controller {
 public:
  FakeController(esp_cxx::QueueSetEventManager* event_manager)
    : Controller(event_manager) {
  }

  HalfDuplexChannel* hvac_control() override { return &mock_hvac_control; }
  HalfDuplexChannel* thermostat() override { return &mock_thermostat; }

  MockHalfDuplexChannel mock_hvac_control;
  MockHalfDuplexChannel mock_thermostat;

  // Expose for use in testing.
  using Controller::OnThermostatPacket;
  using Controller::OnHvacControlPacket;
};

class ControllerTest : public ::testing::Test {
 protected:
  esp_cxx::QueueSetEventManager event_manager_{10};
  FakeController controller_{&event_manager_};
};

// * Sends connect packet.
TEST_F(ControllerTest, Start) {
  EXPECT_CALL(controller_.mock_hvac_control, Start());
  EXPECT_CALL(controller_.mock_thermostat, Start());
  EXPECT_CALL(controller_.mock_hvac_control, EnqueuePacket(NotNull()));

  controller_.Start();

  event_manager_.Run([=]{event_manager_.Quit();});
  event_manager_.Loop();
}

// * Ignores junk/incomplete/invalid packets.
// * Acks a connect.
// * Acks an extended connect.
// * Acks an info request.
// * Merges an update.
TEST_F(ControllerTest, OnThermostatPacket) {
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kConnectAck)))
             );
  controller_.OnThermostatPacket(ConnectPacket::Create());
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kExtendedConnectAck)))
             );
  controller_.OnThermostatPacket(ExtendedConnectPacket::Create());
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  StoredHvacSettings settings;
  settings.Set(Power::kOn);
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kUpdateAck)))
             );
  StoredHvacSettings orig_settings = controller_.GetSettings();
  controller_.OnThermostatPacket(UpdatePacket::Create(settings));
  StoredHvacSettings new_settings = controller_.GetSettings();
  EXPECT_NE(orig_settings.encoded_bytes(), new_settings.encoded_bytes());
  EXPECT_EQ(orig_settings.Get<Power>(), Power::kOn);
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  std::unique_ptr<Cn105Packet> settings_ack = InfoAckPacket::Create(controller_.GetSettings());
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(
                      AllOf(Property(&Cn105Packet::type, PacketType::kInfoAck),
                            Property(&Cn105Packet::data_str, settings_ack->data_str())
                           )
                      )
                  )
             );
  controller_.OnThermostatPacket(InfoPacket::Create(CommandType::kSettings));
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  StoredExtendedSettings extended_settings;
  static constexpr HalfDegreeTemp kTestRoomTemp(20, true);
  extended_settings.SetRoomTemp(kTestRoomTemp);
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kUpdateAck)))
             );
  StoredExtendedSettings orig_extended_settings = controller_.GetExtendedSettings();
  controller_.OnThermostatPacket(UpdatePacket::Create(extended_settings));
  StoredExtendedSettings new_extended_settings = controller_.GetExtendedSettings();
  EXPECT_NE(orig_extended_settings.encoded_bytes(),
            new_extended_settings.encoded_bytes());
  EXPECT_EQ(new_extended_settings.GetRoomTemp().value(), kTestRoomTemp);
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  std::unique_ptr<Cn105Packet> extended_settings_ack =
      InfoAckPacket::Create(controller_.GetExtendedSettings());
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(
                      AllOf(Property(&Cn105Packet::type, PacketType::kInfoAck),
                            Property(&Cn105Packet::data_str, extended_settings_ack->data_str())
                           )
                      )
                  )
             );
  controller_.OnThermostatPacket(InfoPacket::Create(CommandType::kExtendedSettings));
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kInfoAck)))
             );
  // TODO(awong): Verify the Acked data.
  controller_.OnThermostatPacket(InfoPacket::Create(CommandType::kTimers));
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kInfoAck)))
             );
  // TODO(awong): Verify the Acked data.
  controller_.OnThermostatPacket(InfoPacket::Create(CommandType::kStatus));
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);
}

// * Packets sent to one interace show up in the other, regardless of type.
TEST_F(ControllerTest, PassThru) {
  controller_.set_passthru(true);
  std::array<uint8_t, 2> junk1 = {0x01, 0x02};
  std::array<uint8_t, 2> junk2 = {0x03, 0x04};

  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(Pointee(Property(&Cn105Packet::raw_bytes_str, ElementsAreArray(junk1)))));
  EXPECT_CALL(controller_.mock_hvac_control,
              EnqueuePacket(Pointee(Property(&Cn105Packet::raw_bytes_str, ElementsAreArray(junk2)))));

  std::unique_ptr<Cn105Packet> junk_packet = std::make_unique<Cn105Packet>();
  junk_packet->AppendByte(junk1[0]);
  junk_packet->AppendByte(junk1[1]);
  controller_.OnHvacControlPacket(std::move(junk_packet));

  junk_packet = std::make_unique<Cn105Packet>();
  junk_packet->AppendByte(junk2[0]);
  junk_packet->AppendByte(junk2[1]);
  controller_.OnThermostatPacket(std::move(junk_packet));
}

// * Logs all packets from both interfaces, including junk, incomplete, invalid
//   checksum and bad packet type.
TEST_F(ControllerTest, Logging) {
}

// * Timeout causes an attempt to reconnect.
// * Timeout will occur even if other events are queued.
TEST_F(ControllerTest, Timeout) {
}

// * Normal and Extended settings are pushed to the controller.
TEST_F(ControllerTest, PushSettings) {
  StoredHvacSettings settings;
  settings.Set(Power::kOn);
  std::unique_ptr<Cn105Packet> settings_update = UpdatePacket::Create(settings);
  EXPECT_CALL(controller_.mock_hvac_control,
              EnqueuePacket(
                  Pointee(
                      AllOf(Property(&Cn105Packet::type, PacketType::kUpdate),
                            Property(&Cn105Packet::data_str, settings_update->data_str()))
                      )
                  )
             );
  controller_.PushSettings(settings);

  StoredExtendedSettings extended_settings;
  settings.Set(Power::kOn);
  std::unique_ptr<Cn105Packet> extended_settings_update = UpdatePacket::Create(extended_settings);
  EXPECT_CALL(controller_.mock_hvac_control,
              EnqueuePacket(
                  Pointee(
                      AllOf(Property(&Cn105Packet::type, PacketType::kUpdate),
                            Property(&Cn105Packet::data_str, extended_settings_update->data_str()))
                      )
                  )
             );
  controller_.PushExtendedSettings(extended_settings);

  event_manager_.Run([=]{event_manager_.Quit();});
  event_manager_.Loop();
}

// * State received from hvac unit is stored.
TEST_F(ControllerTest, QueryInfo) {
}

}  // namespace hackvac
