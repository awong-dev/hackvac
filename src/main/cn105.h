#ifndef CN105_H_
#define CN105_H_

#include "half_duplex_channel.h"

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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "half_duplex_channel.h"

namespace hackvac {

class Controller {
 public:
  Controller();
  ~Controller();

  // Starts the message processing.
  void Init();

  // Mode changes.
  void set_passthru(bool is_passthru) { is_passthru_ = is_passthru; }
  bool is_passthru() { return is_passthru_; }

 private:
  // Runs on the |hvac_control_| channel's message pump task.
  void OnHvacControlPacket(std::unique_ptr<Cn105Packet> hvac_packet);

  // Runs on the |thermostat_| channel's message pump task.
  void OnThermostatPacket(std::unique_ptr<Cn105Packet> thermostat_packet);

  // Generates an Ack for an info packet.
  std::unique_ptr<Cn105Packet> CreateInfoAck(InfoPacket info);

  // Sets the state.
  bool is_passthru_ = false;

  // Channel talking to the HVAC control unit.
  HalfDuplexChannel hvac_control_;

  // Channel talking to the thermosat.
  HalfDuplexChannel thermostat_;

  // Ensures locked access to shared fields.
  class SharedData {
   public:
    SharedData();
    HvacSettings GetHvacSettings() const;
    void SetHvacSettings(const HvacSettings& hvac_settings);

   private:
    mutable portMUX_TYPE mux_;
    // Current settings of the HVAC unit.
    HvacSettings hvac_settings_;
  };

  SharedData shared_data_;
};

}  // namespace hackvac

#endif  // CN105_H_
