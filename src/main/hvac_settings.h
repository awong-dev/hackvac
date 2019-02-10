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

// TODO(awong):
//   I'm betting this is settings commands types that are effectively a common
//   enum used by both Info/Update. Note that in update, there is a similar format
//   but with 
//     0x1 == normal update
//     0x7 == extended settings update
//   and those updates are non-overlapping with these.
enum class CommandType : uint8_t {
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
    : encoded_temp_(temp * 10 + (plus_half ? 5 : 0)) {
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

namespace internal {
enum class SettingsBitfield : uint8_t {
  kPowerFlag = 0x01,
  kModeFlag = 0x02,
  kTempFlag = 0x04,
  kFanFlag = 0x08,
  kVaneFlag = 0x10,
  kWideVaneFlag,  // 0x01 of bitfield byte #2
};

enum class ExtendedSettingsBitfield : uint8_t {
  kRoomTempFlag = 0x01,
};

template <SettingsBitfield field>
struct Bitfield {
  static bool Has(const uint8_t* data_ptr) {
      return data_ptr[1] & static_cast<uint8_t>(field);
  }
  static void Set(uint8_t* data_ptr) {
      data_ptr[1] |= static_cast<uint8_t>(field);
  }
  static void Unset(uint8_t* data_ptr) {
      data_ptr[1] &= ~static_cast<uint8_t>(field);
  }
};

template <>
struct Bitfield<SettingsBitfield::kWideVaneFlag> {
  static bool Has(const uint8_t* data_ptr) { return data_ptr[2] & 0x1; }
  static void Set(uint8_t* data_ptr) { data_ptr[2] |= 0x1; }
  static void Unset(uint8_t* data_ptr) { data_ptr[2] &= 0x1; }
};

template <typename T> struct ExtractConfig;
template <> struct ExtractConfig<Power> {
  constexpr static SettingsBitfield kBitfield = SettingsBitfield::kPowerFlag;
  constexpr static int kDataPos = 3;
};
template <> struct ExtractConfig<Mode> {
  constexpr static SettingsBitfield kBitfield = SettingsBitfield::kModeFlag;
  constexpr static int kDataPos = 4;
};
template <> struct ExtractConfig<Fan> {
  constexpr static SettingsBitfield kBitfield = SettingsBitfield::kFanFlag;
  constexpr static int kDataPos = 6;
};
template <> struct ExtractConfig<Vane> {
  constexpr static SettingsBitfield kBitfield = SettingsBitfield::kVaneFlag;
  constexpr static int kDataPos = 7;
};
template <> struct ExtractConfig<WideVane> {
  constexpr static SettingsBitfield kBitfield = SettingsBitfield::kWideVaneFlag;
  constexpr static int kDataPos = 10;
};
}  // namespace internal

// Normal settings for the HVAC controller.
//
// This class stores the Power, Mode, TargetTemp, Fan, Vane, and WideVane 
// settings using the wire format for the Info and Update packets in the
// CN105 protocol.
//
// Control Update (byte0)
//   0x01 = update all standard settings. Next 2 bytes are bitfields.
//            byte1 = power 0x1, mode 0x2, temp 0x4, fan 0x8, vane 0x10, dir 0x80
//            byte2 = wiadevane 0x1 
//          Data for each is in a corresponding byte.
//            byte3 = Power
//            byte4 = Mode
//            byte5 = Temp (0x00 for temp seems to mean "max" and not 31-celcius)
//            byte6 = Fan
//            byte7 = Vane
//            byte10 = Dir
class HvacSettings {
 public:
  static const HalfDegreeTemp kMaxTemp;
  static const HalfDegreeTemp kMinTemp;

  explicit HvacSettings(uint8_t* raw_data) : data_ptr_(raw_data) {
    // TODO(awong): Assert error is kSetSettings?
  }

  template <typename T>
  std::optional<T> Get() const {
    if (!Bitfield<ExtractConfig<T>::kBitfield>::Has(data_ptr_)) {
      return {};
    }
    return static_cast<T>(data_ptr_[ExtractConfig<T>::kDataPos]);
  }

  template <typename T>
  void Set(std::optional<T> setting) const {
    constexpr SettingsBitfield field = ExtractConfig<T>::kBitfield;
    if (setting) {
      Set(setting.value());
    } else {
      Bitfield<field>::Unset(data_ptr_);
      data_ptr_[ExtractConfig<T>::kDataPos] = 0x00;
    }
  }

  template <typename T>
  void Set(T setting) const {
    constexpr SettingsBitfield field = ExtractConfig<T>::kBitfield;
    Bitfield<field>::Set(data_ptr_);
    data_ptr_[ExtractConfig<T>::kDataPos] = static_cast<uint8_t>(setting);
  }

  std::optional<HalfDegreeTemp> GetTargetTemp() const;
  void SetTargetTemp(std::optional<HalfDegreeTemp> target_temp);

  const uint8_t* data_pointer() const { return data_ptr_; }

 protected:
  void set_data_pointer(uint8_t* ptr) { data_ptr_ = ptr; }

 private:
  using SettingsBitfield = internal::SettingsBitfield;
  template <SettingsBitfield field> using Bitfield = internal::Bitfield<field>;
  template <typename T> using ExtractConfig = internal::ExtractConfig<T>;

  uint8_t* data_ptr_ = nullptr;
};

// HvacSettings that provides its own storage. This is likely most often
// used to actually store the settings whereas HvacSettings can be used
// to parse a chunk of data out of a received packet.
class StoredHvacSettings : public HvacSettings {
 public:
  StoredHvacSettings() : HvacSettings(nullptr) {
    set_data_pointer(data_.data());
  }

  StoredHvacSettings& operator=(const HvacSettings& rhs) {
    memcpy(data_.begin(), rhs.data_pointer(), data_.size());
    return *this;
  }

  void MergeUpdate(const HvacSettings& settings_update) {
    if (auto new_value = settings_update.Get<Power>()) Set(new_value.value());
    if (auto new_value = settings_update.Get<Mode>()) Set(new_value.value());
    if (auto new_value = settings_update.Get<Fan>()) Set(new_value.value());
    if (auto new_value = settings_update.Get<Vane>()) Set(new_value.value());
    if (auto new_value = settings_update.Get<WideVane>()) Set(new_value.value());
  }
    
  // Returns the raw wireformat data bytes for an update packet. The
  // array is only valid until the next mutation of this object. Best
  // to copy th e values out immediately.
  const std::array<uint8_t, 16>& encoded_bytes() const { return data_; }

 private:
  // Settings data stores as expected in the wire format.
  std::array<uint8_t, 16> data_ = {};
};

//            byte0 = room temp 0x1, 
//            byte1 = ???
//            room temp is first byte after.
//            NOTE: MHK1 sends 0x80 for data if setting is nonsense (negative)
//                  yielding 0x00 on byte0 bitflag.
class ExtendedSettings {
 public:
  static const HalfDegreeTemp kMaxRoomTemp;
  static const HalfDegreeTemp kMinRoomTemp;

  explicit ExtendedSettings(uint8_t* raw_data) : data_ptr_(raw_data) {}

  std::optional<HalfDegreeTemp> GetRoomTemp() const;

  const uint8_t* data_pointer() const { return data_ptr_; }

 protected:
  void set_data_pointer(uint8_t* ptr) { data_ptr_ = ptr; }

 private:
  uint8_t* data_ptr_ = nullptr;
};

class StoredExtendedSettings : public ExtendedSettings {
 public:
  StoredExtendedSettings() : ExtendedSettings(nullptr) {
    set_data_pointer(data_.data());
  }

  StoredExtendedSettings& operator=(const ExtendedSettings& rhs) {
    memcpy(data_.begin(), rhs.data_pointer(), data_.size());
    return *this;
  }

  const std::array<uint8_t, 16>& encoded_bytes() const { return data_; }

 private:
  std::array<uint8_t, 16> data_ = {};
};

}  // namespace hackvac

#endif  // HVAC_SETTINGS_H_
