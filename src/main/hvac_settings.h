#ifndef HVAC_SETTINGS_H_
#define HVAC_SETTINGS_H_

#include <array>
#include <cstdint>

#include "esp_cxx/cxx17hack.h"

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
  // TODO(awong): This Isee stuff seems wrong? Comes from
  // https://github.com/SwiCago/HeatPump/blob/master/src/HeatPump.cpp
  kIseeHeat = kHeat + 0x08,
  kIseeDry = kDry + 0x08,
  kIseeCool = kCool + 0x08,
  kIseeFan = kFan + 0x08,
  kIseeAuto = kAuto + 0x08,
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

// TODO(awong):
//   I'm betting this is settings commands types that are effectively a common
//   enum used by both Info/Update. Note that in update, there is a similar format
//   but with 
//     0x1 == normal update
//     0x7 == extended settings update
//   and those updates are non-overlapping with these.
//
// TODO(awong): Rename field type.
enum class InfoType : uint8_t {
  kSetSettings = 0x01,
  kSettings = 0x02,
  kExtendedSettings = 0x03,
  // 0x04 is unknown
  kTimers = 0x05,
  kStatus = 0x06,
  kSetExtendedSettings = 0x07,
  kEnterStandby = 0x09,  // maybe? TODO(awong): Remove if we can't figure out what this is.
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

// Normal settings for the HVAC controller.
//
// This class stores the Power, Mode, TargetTemp, Fan, Vane, and WideVane 
// settings using the wire format for the Info and Update packets in the
// CN105 protocol.
//
// TODO(awong): Set the bitfields.
class HvacSettings {
 public:
  static const HalfDegreeTemp kMaxTemp;
  static const HalfDegreeTemp kMinTemp;

  explicit HvacSettings(uint8_t* raw_data) : data_ptr_(raw_data) {
  }

  std::optional<Power> GetPower() const;
  void SetPower(Power power);

  std::optional<Mode> GetMode() const;
  void SetMode(Mode mode);

  std::optional<HalfDegreeTemp> GetTargetTemp() const;
  void SetTargetTemp(HalfDegreeTemp target_temp);

  std::optional<Fan> GetFan() const;
  void SetFan(Fan fan);

  std::optional<Vane> GetVane() const;
  void SetVane(Vane vane);

  std::optional<WideVane> GetWideVane() const;
  void GetWideVane(WideVane wide_vane);

 private:
  uint8_t* data_ptr_ = nullptr;
};

// HvacSettings that provides its own storage. This is likely most often
// used to actually store the settings whereas HvacSettings can be used
// to parse a chunk of data out of a received packet.
class StoredHvacSettings : public HvacSettings {
 public:
  StoredHvacSettings() : HvacSettings(data_.data()) {
  }

  void MergeUpdate(const HvacSettings& settings_update) {
  }
    
  // Returns the raw wireformat data bytes for an update packet. The
  // array is only valid until the next mutation of this object. Best
  // to copy the values out immediately.
  const std::array<uint8_t, 16>& encoded_bytes() const { return data_; }

 private:
  // Settings data stores as expected in the wire format.
  std::array<uint8_t, 16> data_ = {};
};

struct ExtendedSettings {
 public:
  const std::array<uint8_t, 16>& encoded_bytes() const { return data_; }

  RoomTemp room_temp;
 private:
  uint8_t* ptr_ = nullptr;
  std::array<uint8_t, 16> data_ = {};
};

}  // namespace hackvac

#endif  // HVAC_SETTINGS_H_
