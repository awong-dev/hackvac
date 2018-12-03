#ifndef HVAC_SETTINGS_H_
#define HVAC_SETTINGS_H_

// This contains the enums and classes for managing HVAC settings.
// Note that the enum values are overloaded to be the actual byte
// values stored in the control protocol. This is a small abstraction
// break but removes a heck of a lot of redundant code.

namespace hackvac {

enum class Power : uint8_t {
  kOff = 0x00,
  kOn = 0x01,
};

enum class Mode : uint8_t {
  kHeat = 0x01,
  kDry = 0x02,
  kCool = 0x03,
  kFan = 0x07,
  kAuto = 0x08,
};

enum class TargetTemp : uint8_t {
  kMaxTemp = 0x00,
  k30C = 0x01,
  k29C = 0x02,
  k28C = 0x03,
  k27C = 0x04,
  k26C = 0x05,
  k25C = 0x06,
  k24C = 0x07,
  k23C = 0x08,
  k22C = 0x09,
  k21C = 0x0A,
  k20C = 0x0B,
  k19C = 0x0C,
  k18C = 0x0D,
  k17C = 0x0E,
  kMinTemp = 0x0F,
};

enum class Fan : uint8_t {
  kAuto = 0x00,
  kQuiet = 0x01,
  kPower1 = 0x02,
  kPower2 = 0x03,

  // TODO(awong): 0x04 value is skipped in SwiCago's implementation mitsi.py. Test.
  //  https://github.com/hadleyrich/MQMitsi/blob/master/mitsi.py#L109
  //  https://github.com/SwiCago/HeatPump/blob/master/src/HeatPump.h#L129
  kPower3 = 0x04,

  kPower4 = 0x05,
  kPower5 = 0x06,
};

enum class Vane : uint8_t {
  kAuto = 0x00,
  kPower1 = 0x01,
  kPower2 = 0x02,
  kPower3 = 0x03,
  kPower4 = 0x04,
  kPower5 = 0x05,
  kSwing = 0x07,
};

// From https://github.com/hadleyrich/MQMitsi/blob/master/mitsi.py#L95
enum class WideVane : uint8_t {
  kFarLeft = 0x01,      // <<
  kLeft = 0x02,         // <
  kCenter = 0x03,       // |
  kRight = 0x04,        // >
  kFarRight = 0x05,     // >>
  kLeftAndRight = 0x08, // <> What is this?
  kSwing = 0x0c,        //
};

enum class RoomTemp : uint8_t {
  k10C = 0x00,
  k11C = 0x01,
  k12C = 0x02,
  k13C = 0x03,
  k14C = 0x04,
  k15C = 0x05,
  k16C = 0x06,
  k17C = 0x07,
  k18C = 0x08,
  k19C = 0x09,
  k20C = 0x0A,
  k21C = 0x0B,
  k22C = 0x0C,
  k23C = 0x0D,
  k24C = 0x0E,
  k25C = 0x0F,
  k26C = 0x0F,
  k27C = 0x0F,
  k28C = 0x0F,
  k29C = 0x0F,
  k30C = 0x0F,
  k31C = 0x0F,
  k32C = 0x0F,
  k33C = 0x0F,
  k34C = 0x0F,
  k35C = 0x0F,
  k36C = 0x0F,
  k37C = 0x0F,
  k38C = 0x0F,
  k39C = 0x0F,
  k40C = 0x0F,
  k41C = 0x0F,
};

enum class UpdateType : uint8_t {
  // Used for power, mode, target temp, fan, vane, direction.
  kNormalSettings = 0x01,

  // Used for room temperature.
  kExtendedSettings = 0x07,
};

enum class InfoType : uint8_t {
  kSettings = 0x02,
  kExtendedSettings = 0x03,
  // 0x04 is unknown
  kTimers = 0x05,
  kStatus = 0x06,
  kEnterStandby = 0x09,  // maybe?
};

struct HvacSettings {
  HvacSettings()
    : power(Power::kOff),
      mode(Mode::kAuto),
      target_temp(TargetTemp::k20C),
      fan(Fan::kAuto),
      vane(Vane::kAuto),
      wide_vane(WideVane::kCenter) {
  }

  Power power;
  Mode mode;
  TargetTemp target_temp;
  Fan fan;
  Vane vane;
  WideVane wide_vane;
};

struct ExtendedSettings {
  RoomTemp room_temp;
};

}  // namespace hackvac

#endif  // HVAC_SETTINGS_H_
