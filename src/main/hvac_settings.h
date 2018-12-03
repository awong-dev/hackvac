#ifndef HVAC_SETTINGS_H_
#define HVAC_SETTINGS_H_

#include <cstdint>

#include <array>

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
  k26C = 0x10,
  k27C = 0x11,
  k28C = 0x11,
  k29C = 0x12,
  k30C = 0x13,
  k31C = 0x14,
  k32C = 0x15,
  k33C = 0x16,
  k34C = 0x17,
  k35C = 0x18,
  k36C = 0x19,
  k37C = 0x1A,
  k38C = 0x1B,
  k39C = 0x1C,
  k40C = 0x1D,
  k41C = 0x1E,
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

// Represents temperatures in celcius in 0.5 degree increments. 
class HalfDegreeTemp {
 public:
  HalfDegreeTemp(int temp, bool plus_half)
    : encoded_temp_(temp * 10 + plus_half ? 5 : 0) {
  }

  bool operator< (const HalfDegreeTemp& rhs) const {
    return encoded_temp_ < rhs.encoded_temp_;
  }

  // The Cn105 wireformat encodes 1/2 degree increments as
  // as a single byte at 2-times the temperature value. The
  // byte also seems to always have the highbit set.
  static HalfDegreeTemp ParseEncoded(uint8_t encoded_temp) {
    return HalfDegreeTemp(encoded_temp & 0x7F);
  }
  uint8_t encoded_temp() const { return 0x80 | encoded_temp_; }

  bool is_half_degree() const { return encoded_temp_ % 2 != 0; }
  int whole_degree() const { return encoded_temp_ / 2; }

 private:
  explicit HalfDegreeTemp(uint8_t encoded_temp) : encoded_temp_(encoded_temp & 0x7F) {
  }

  // Temperature stored as celcius * 2 to allow 1/2 degree resolution.
  uint8_t encoded_temp_;
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

// Normal settings for the HVAC controller.
//
// This class stores the Power, Mode, TargetTemp, Fan, Vane, and WideVane 
// settings using the wire format for the Info and Update packets in the
// CN105 protocol.
class HvacSettingsData {
 public:
  static const HalfDegreeTemp kMaxTemp;
  static const HalfDegreeTemp kMinTemp;

  // Returns the raw wireformat data bytes for an update packet. The
  // array is only valid until the next mutation of this object. Best
  // to copy the values out immediately.
  const std::array<uint8_t, 16>& GetDateForUpdate();

  // No setter on this.
  bool GetHasISee() const;

  Power GetPower() const;
  void SetPower(Power power);

  Mode GetMode() const;
  void SetMode(Mode mode);

  HalfDegreeTemp GetTargetTemp() const;
  void SetTargetTemp(HalfDegreeTemp target_temp);

  Fan GetFan() const;
  void SetFan(Fan fan);

  Vane GetVane() const;
  void SetVane(Vane vane);

  WideVane GetWideVane() const;
  void GetWideVane(WideVane wide_vane);

 private:
  // Settings data stores as expected in the wire format.
  std::array<uint8_t, 16> data_ = {};
};

struct ExtendedSettings {
  RoomTemp room_temp;
};

}  // namespace hackvac

#endif  // HVAC_SETTINGS_H_
