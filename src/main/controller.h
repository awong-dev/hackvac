#ifndef CN105_H_
#define CN105_H_

#include <deque>

#include "half_duplex_channel.h"
#include "cn105_protocol.h"

#include "esp_cxx/cxx17hack.h"
#include "esp_cxx/mutex.h"
#include "esp_cxx/data_logger.h"

#include "gtest/gtest_prod.h"

// This class is designed to control a Mitsubishi CN105 serial control
// interface. It can either MiTM a signal to log/modify commands, or it
// can just take over and send its own commands.
//
// CN105 is a custom checksumed protocol transported over a 2400 BAUD serial
// connection.
//
// ?? The protocol seems to be half-duplex, needing a 10ms pause between send
// ?? and receive.
//
// This class configures 2 UARTs with the expectation that it is sitting
// between the CN105 connector on the Heatpump control board, and the
// Mitsubishi-PAC-US444CN-1 Mitsubishi Thermostat Interface (tstat).
//
// The tstat is an optional client. If it isn't connected, no big deal as
// this module is the true client.
//
// Both UARTs are configured in event mode with the ESP-IDF uart drivers
// placing bytes into FreeRTOS event queues. Each of these queues is processed
// by their own FreeRTOS task. The tasks parses the messages into a CN105
// packet and forwards them along.
//
// At the start of each packet, the task with take an atomic snapshot of the
// Controller object's packet modification configuration. That ensures that each
// packet is internally consistent. It also means updates to the Controller object
// only get latched into effect at packet boundaries.

#include "esp_cxx/event_manager.h"
#include "esp_cxx/task.h"
#include "esp_cxx/test.h"
#include "esp_cxx/queue.h"

#include "half_duplex_channel.h"

namespace hackvac {

class Controller {
 public:
  explicit Controller(esp_cxx::QueueSetEventManager* event_manager);
  ESPCXX_MOCKABLE ~Controller();

  // Starts the message processing.
  void Start();

  // Mode changes.
  void set_passthru(bool is_passthru) { is_passthru_ = is_passthru; }
  bool is_passthru() { return is_passthru_; }

  void SetTemperature(HalfDegreeTemp temp);

  StoredHvacSettings GetSettings() const {
    return shared_data_.GetStoredHvacSettings();
  }

  StoredExtendedSettings GetExtendedSettings() const {
    return shared_data_.GetExtendedSettings();
  }

 private:
  friend class FakeController;

  enum class Command : uint8_t {
    kConnect,
    kQuerySettings,
    kQueryExtendedSettings,
    kPushSettings,
    kPushExtendedSettings,
  };

  // Accessors that allow for unittest to dependency inject.
  ESPCXX_MOCKABLE HalfDuplexChannel* hvac_control() { return &hvac_control_; }
  ESPCXX_MOCKABLE HalfDuplexChannel* thermostat() { return &thermostat_; }

  // Runs on the |hvac_control_| channel's message pump task.
  void OnHvacControlPacket(std::unique_ptr<Cn105Packet> hvac_packet);

  // Runs on the |thermostat_| channel's message pump task.
  void OnThermostatPacket(std::unique_ptr<Cn105Packet> thermostat_packet);

  // Adds a command to the queue and attempts to run it.
  void ScheduleCommand(Command command);

  // Starts the next command in |command_queue_|. This the start of the logical
  // protocol actions and is invoked either on a timer, as a sideffect of a
  // public method call, or in response a packet or uart event.
  void ExecuteNextCommand();

  // Generates an Ack for an info packet.
  std::unique_ptr<Cn105Packet> CreateInfoAck(InfoPacket info);

  // Sets the state.
  bool is_passthru_ = false;

  // Event manager for handling all incoming data.
  esp_cxx::QueueSetEventManager* event_manager_;

  // Channel talking to the HVAC control unit.
  HalfDuplexChannel hvac_control_;

  // Channel talking to the thermosat.
  HalfDuplexChannel thermostat_;

  // Commands to run.
  std::deque<Command> command_queue_;

  // The number of commands sent. Use for tracking timeouts.
  unsigned int command_number_ = 0;

  // Whether or not the current command has received an ack.
  bool is_command_oustanding_ = false;

  // Asynchronous logger to track protocol interactions.
  esp_cxx::DataLogger<std::unique_ptr<Cn105Packet>, 50, &Cn105Packet::LogPacketThunk> packet_logger_{"packets"};

  // Ensures locked access to shared fields.
  class SharedData {
   public:
    StoredHvacSettings GetStoredHvacSettings() const;
    void SetStoredHvacSettings(const HvacSettings& hvac_settings);
    StoredExtendedSettings GetExtendedSettings() const;
    void SetExtendedSettings(const ExtendedSettings& extended_settings);

   private:
    mutable esp_cxx::Mutex mutex_;

    // Current settings to push to the hvac controller.
    StoredHvacSettings hvac_settings_;

    // Current extended settings to push to the hvac controller.
    StoredExtendedSettings extended_settings_;
  };

  // Hvac state being accessed by multiple tasks.
  SharedData shared_data_;
};

}  // namespace hackvac

#endif  // CN105_H_
