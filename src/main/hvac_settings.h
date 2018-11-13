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

}  // namespace hackvac

#endif  // HVAC_SETTINGS_H_
