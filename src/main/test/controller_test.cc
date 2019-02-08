#include "../controller.h"

#include "esp_cxx/event_manager.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using testing::AllOf;
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

  using Controller::OnThermostatPacket;
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

  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kUpdateAck)))
             );
  StoredHvacSettings settings;
  controller_.OnThermostatPacket(UpdatePacket::Create(settings));
  // TODO(awong): Verify the settings make it into the controller.
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kInfoAck)))
             );
  // TODO(awong): Verify the Acked data.
  controller_.OnThermostatPacket(InfoPacket::Create(CommandType::kSettings));
  Mock::VerifyAndClearExpectations(&controller_.mock_thermostat);

  EXPECT_CALL(controller_.mock_thermostat,
              EnqueuePacket(
                  Pointee(Property(&Cn105Packet::type, PacketType::kInfoAck)))
             );
  // TODO(awong): Verify the Acked data.
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
}

// * State received from hvac unit is stored.
TEST_F(ControllerTest, QueryInfo) {
}

}  // namespace hackvac
