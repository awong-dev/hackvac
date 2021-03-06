#include "../controller.h"

#include "esp_cxx/event_manager.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using testing::AllOf;
using testing::AtLeast;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Mock;
using testing::NotNull;
using testing::Property;
using testing::_;

namespace hackvac {
namespace {
constexpr std::array<uint8_t, 2> kJunk1 = {0x01, 0x02};
constexpr std::array<uint8_t, 2> kJunk2 = {0x03, 0x04};
constexpr std::array<uint8_t, 8> kConnect = { 0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa8 };
constexpr std::array<uint8_t, 6> kConnectIncomplete = { 0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca };
constexpr std::array<uint8_t, 8> kConnectBadChecksum = { 0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa6 };
constexpr std::array<uint8_t, 8> kBadType = { 0xfc, 0x59, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa9 };

template <size_t n>
std::unique_ptr<Cn105Packet> MakePacket(const std::array<uint8_t, n>& data) {
  auto packet = std::make_unique<Cn105Packet>();
  for (auto byte : data) {
    packet->AppendByte(byte);
  }
  return packet;
}

}  // namespace

class MockPacketLogger : public esp_cxx::DataLogger<std::unique_ptr<Cn105Packet>> {
 public:
  MOCK_METHOD2(Log, void(const char*, std::unique_ptr<Cn105Packet>));
};

class MockHalfDuplexChannel : public HalfDuplexChannel {
 public:
  MockHalfDuplexChannel()
    : HalfDuplexChannel(nullptr, esp_cxx::Uart::Chip::kInvalid, {}, {}, {}) {}
  MOCK_METHOD0(Start, void());
  MOCK_METHOD1(EnqueuePacket, void(std::unique_ptr<Cn105Packet>));
};

class FakeController : public Controller {
 public:
  FakeController(esp_cxx::QueueSetEventManager* event_manager,
                 PacketLoggerType* packet_logger)
    : Controller(event_manager, packet_logger) {
  }

  HalfDuplexChannel* hvac_control() override { return &mock_hvac_control; }
  HalfDuplexChannel* thermostat() override { return &mock_thermostat; }

  MockHalfDuplexChannel mock_hvac_control;
  MockHalfDuplexChannel mock_thermostat;

  // Expose for use in testing.
  using Controller::OnThermostatPacket;
  using Controller::OnHvacControlPacket;
  using Controller::is_command_oustanding_;
};

class ControllerTest : public ::testing::Test {
 protected:
  // Most tests do not want to verify the packet logger behavior.
  // Call this function to make the mock just read and drop messages.
  void IgnoreLogCalls() {
    EXPECT_CALL(mock_packet_logger_, Log(_,_)).Times(AtLeast(0));
  }

  esp_cxx::QueueSetEventManager event_manager_{10};
  testing::StrictMock<MockPacketLogger> mock_packet_logger_;
  FakeController controller_{&event_manager_, &mock_packet_logger_};
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
  IgnoreLogCalls();

  // Respond to connect packet.
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kConnectAck)))
             );
  controller_.OnThermostatPacket(ConnectPacket::Create());
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  // Respond to an extended connect packet.
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kExtendedConnectAck)))
             );
  controller_.OnThermostatPacket(ExtendedConnectPacket::Create());
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  // Respond to a settings update packet.
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

  // Responds to a settings query packet.
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

  // Respond to a room tempeature set packet.
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

  // Respond to an exteded settings query packet.
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

  // Respond to a timer query packet.
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kInfoAck)))
             );
  // TODO(awong): Verify the Acked data.
  controller_.OnThermostatPacket(InfoPacket::Create(CommandType::kTimers));
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  // Respond to a status query packet.
  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kInfoAck)))
             );
  // TODO(awong): Verify the Acked data.
  controller_.OnThermostatPacket(InfoPacket::Create(CommandType::kStatus));
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);
}

TEST_F(ControllerTest, OnHvacControlPacket_ClearsOutstanding) {
  IgnoreLogCalls();

  // Basic test of acks that don't do anything.
  controller_.is_command_oustanding_ = true;
  controller_.OnHvacControlPacket(ConnectAckPacket::Create());
  EXPECT_FALSE(controller_.is_command_oustanding_);

  controller_.is_command_oustanding_ = true;
  controller_.OnHvacControlPacket(ExtendedConnectAckPacket::Create());
  EXPECT_FALSE(controller_.is_command_oustanding_);

  controller_.is_command_oustanding_ = true;
  controller_.OnHvacControlPacket(UpdateAckPacket::Create());
  EXPECT_FALSE(controller_.is_command_oustanding_);

  controller_.is_command_oustanding_ = true;
  controller_.OnHvacControlPacket(InfoAckPacket::Create(controller_.GetSettings()));
  EXPECT_FALSE(controller_.is_command_oustanding_);
}

//  * data from kInfoAck is merged.
TEST_F(ControllerTest, OnHvacControlPacket_MergesInfoAckData) {
  // TODO(awong): Impelment.
}

// Junk packets are ignored.
TEST_F(ControllerTest, OnHvacControlPacket_IgnoreJunk) {
  IgnoreLogCalls();

  controller_.is_command_oustanding_ = true;
  controller_.OnHvacControlPacket(MakePacket(kJunk1));
  EXPECT_TRUE(controller_.is_command_oustanding_);
  // The main test is no mocks get triggered.
}

// Incomplete packets trigger a reconnect
TEST_F(ControllerTest, OnHvacControlPacket_IncompleteReconnects) {
  IgnoreLogCalls();

  auto expected = ConnectPacket::Create();
  EXPECT_CALL(controller_.mock_hvac_control,
              EnqueuePacket(Pointee(Property(&Cn105Packet::raw_bytes_str, expected->raw_bytes_str()))));
  controller_.OnHvacControlPacket(MakePacket(kConnectIncomplete));
}

// * Packets sent to one interace show up in the other, regardless of type.
TEST_F(ControllerTest, PassThru) {
  IgnoreLogCalls();

  EXPECT_FALSE(controller_.is_passthru());
  controller_.set_passthru(true);
  EXPECT_TRUE(controller_.is_passthru());

  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(Pointee(Property(&Cn105Packet::raw_bytes_str, ElementsAreArray(kJunk1)))));
  EXPECT_CALL(controller_.mock_hvac_control,
              EnqueuePacket(Pointee(Property(&Cn105Packet::raw_bytes_str, ElementsAreArray(kJunk2)))));

  controller_.OnHvacControlPacket(MakePacket(kJunk1));
  controller_.OnThermostatPacket(MakePacket(kJunk2));
}

// * Logs all packets from both interfaces, including
//     * junk
//     * incomplete
//     * invalid checksum
//     * bad packet type
// * Logs in pass-thru mode.
TEST_F(ControllerTest, Logging) {
  // Not testing what happens with a packet, so cut the dependency on the channel
  // with a super permissive mock.
  EXPECT_CALL(controller_.mock_thermostat, EnqueuePacket(_)).Times(AtLeast(0));
  EXPECT_CALL(controller_.mock_hvac_control, EnqueuePacket(_)).Times(AtLeast(0));

  // Write a little helper lambda for doing the test.
  auto do_test = [&](auto data) {
    std::unique_ptr<Cn105Packet> packet = MakePacket(data);
    EXPECT_CALL(mock_packet_logger_,
                Log(Controller::kHvacRxTag,
                    Property(&std::unique_ptr<Cn105Packet>::get, packet.get())));
    controller_.OnHvacControlPacket(std::move(packet));
    testing::Mock::VerifyAndClear(&mock_packet_logger_);

    packet = MakePacket(data);
    EXPECT_CALL(mock_packet_logger_,
                Log(Controller::kTstatRxTag, 
                    Property(&std::unique_ptr<Cn105Packet>::get, packet.get())));
    controller_.OnThermostatPacket(std::move(packet));
    testing::Mock::VerifyAndClear(&mock_packet_logger_);
  };

  // Actually invoke the test cases.
  { SCOPED_TRACE("junk"); do_test(kJunk1); }
  { SCOPED_TRACE("good"); do_test(kConnect); }
  { SCOPED_TRACE("incomplete"); do_test(kConnectIncomplete); }
  { SCOPED_TRACE("checksum"); do_test(kConnectBadChecksum); }
  { SCOPED_TRACE("type"); do_test(kBadType); }

  // Test that packets are logged on receive in pass-thru mode.
  controller_.set_passthru(true);
  { SCOPED_TRACE("passthru junk"); do_test(kJunk1); }
}

// * Timeout causes an attempt to reconnect.
// * Timeout will occur even if other events are queued.
TEST_F(ControllerTest, Timeout) {
  // TODO(awong): Test timeouts.
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

  event_manager_.Run([=]{event_manager_.Quit();});
  event_manager_.Loop();
}

TEST_F(ControllerTest, PushExtendedSettings) {
  StoredExtendedSettings extended_settings;
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

// Queries and stores settings from HVAC settings.
TEST_F(ControllerTest, SyncSettings) {
  // TODO(awong): Implement.
}

// Queries and stores extended settings from HVAC settings.
TEST_F(ControllerTest, SyncExtendedSettings) {
  // TODO(awong): Implement.
}

}  // namespace hackvac
