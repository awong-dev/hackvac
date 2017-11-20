#ifndef CN105_H_
#define CN105_H_

// This class is designed to control a Mitsubishi CN105 serial control
// interface. It can either MiTM a signal to log/modify commands, or it
// can just take over and send its own commands.
//
// CN105 is a custom checksumed protocol transported over a 2400 BAUD serial
// connection.
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
// Controller objects packet modification configuration. That ensures that each
// packet is internally consistent. It also means updates to the Controller object
// only get latched into effect at packet boundaries.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace hackvac {

// The Cn105 serial protocol sends compact, checksumed, byte-oriented packets.
//
// Each packet has the following format:
//  | 0xfc | type | 0x01 | 0x30 | data_len | ...data...  | checksum |
//     0      1      2      3      4          5             last
//  type, data_len, and checksum are all 1 byte.
//
//  0xfc is the start-byte. Useful for synchronization on corruption.
//
//  The 0x01 and 0x30 are unknown extra tags that look constant.
//  Possibly version information or other synchronization markers that can
//  detect endianess?
//
//  https://github.com/SwiCago/HeatPump assumes 22 byte max for full packet
//  but format-wise, data_len can be 255 so max packet size may be 261.
//
//  checksum = (0xfc - sum_bytes(data)) & 0xff;
//
// TODO(awong): Check HEADER_LEN from https://github.com/SwiCago/HeatPump/blob/master/src/HeatPump.h which breaks the format above maybe?
//
// Reference code:
//   https://github.com/hadleyrich/MQMitsi/blob/master/mitsi.py
class Cn105Packet {
  public:
    enum class Type : uint8_t {
      // TODO(awong): the second nibble might be zone info. a == zone 1, b = zone2, etc.
      CONNECT = 0x5a,
      CONNECT_ACK = 0x7a, 

      // Update HVAC control status.
      UPDATE = 0x41, 
      UPDATE_ACK = 0x61, 

      // Requesting info from the HeatPump.
      INFO = 0x42, 
      INFO_ACK = 0x62, 
    };

    // For UPDATE (0x41), the first data byte is the setting to change.
    //   Control Update Extended
    //   0x07 = set current room temperature
    //            byte0 = room temp 0x1, 
    //            byte1 = ???
    //            room temp is first byte after.
    //            NOTE: MHK1 sends 0x80 for data if setting is nonsense (negative)
    //                  yielding 0x00 on byte0 bitflag.
    //
    //   Control Update
    //   0x01 = update all standard settings. Next 2 bytes are bitfields.
    //            byte0 = power 0x1, mode 0x2, temp 0x4, fan 0x8, vane 0x10, dir 0x80
    //            byte1 = wiadevane 0x1 
    //          Data for each is in a corresponding byte.
    //            Power = data + 3
    //            Mode = data + 4
    //            Temp = data + 5
    //            Fan = data + 6
    //            Vane = data + 7
    //            Dir = data + 10

    Cn105Packet();

    // Returns true if packet is complete.
    bool AppendByte(uint8_t byte);

  private:
};
 
class Controller {
  public:
    Controller();
    ~Controller();

    void Init();

    // Mode changes.
    void set_passthru(bool is_passthru) { is_passthru_ = is_passthru; }
    bool is_passthru() { return is_passthru_; }

  private:
    static void Cn105RxThunk(void *pvParameters);
    void Cn105Runloop();

    static void TstatRxThunk(void *pvParameters);
    void TstatRunloop();

    bool is_passthru_;

    TaskHandle_t cn105_rx_task_;
    QueueHandle_t cn105_rx_queue_;

    TaskHandle_t tstat_rx_task_;
    QueueHandle_t tstat_rx_queue_;
};

}  // namespace hackvac

#endif  // CN105_H_
