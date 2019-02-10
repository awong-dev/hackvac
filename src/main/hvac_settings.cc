#include "hvac_settings.h"

namespace hackvac {

const HalfDegreeTemp HvacSettings::kMaxTemp(31, false);
const HalfDegreeTemp HvacSettings::kMinTemp(16, false);

std::optional<HalfDegreeTemp> HvacSettings::GetTargetTemp() const {
  if (!Bitfield<SettingsBitfield::kTempFlag>::Has(data_ptr_)) {
    return {};
  }
  if (data_ptr_[11] != 0x00) {
    return HalfDegreeTemp::ParseEncoded(data_ptr_[11]);
  } else {
    return HalfDegreeTemp(kMaxTemp.whole_degree() - data_ptr_[5], false);
  }
}

void HvacSettings::SetTargetTemp(std::optional<HalfDegreeTemp> target_temp) {
  if (!target_temp) {
    Bitfield<SettingsBitfield::kTempFlag>::Unset(data_ptr_);
    data_ptr_[5] = 0x00;
    data_ptr_[11] = 0x00;
    return;
  }

  Bitfield<SettingsBitfield::kTempFlag>::Set(data_ptr_);

  // Clamp max and min target temp.
  HalfDegreeTemp clamped_temp = target_temp.value();
  if (kMaxTemp < clamped_temp) {
    clamped_temp = kMaxTemp;
  } else if (clamped_temp < kMinTemp) {
    clamped_temp = kMinTemp;
  }

  data_ptr_[5] = kMaxTemp.whole_degree() - clamped_temp.whole_degree();
  if (clamped_temp.is_half_degree()) {
    data_ptr_[11] = clamped_temp.encoded_temp();
  } else {
    data_ptr_[11] = 0x00;
  }
}

const HalfDegreeTemp ExtendedSettings::kMaxRoomTemp(41, false);
const HalfDegreeTemp ExtendedSettings::kMinRoomTemp(10, false);

std::optional<HalfDegreeTemp> ExtendedSettings::GetRoomTemp() const {
  if (!(data_ptr_[0] & static_cast<uint8_t>(internal::ExtendedSettingsBitfield::kRoomTempFlag))) {
    return {};
  }
  if (data_ptr_[6] != 0x00) {
    return HalfDegreeTemp::ParseEncoded(data_ptr_[6]);
  } else {
    return HalfDegreeTemp(data_ptr_[3] + kMinRoomTemp.whole_degree(), false);
  }
}

void ExtendedSettings::SetRoomTemp(HalfDegreeTemp temp) {
  data_ptr_[0] |= static_cast<uint8_t>(internal::ExtendedSettingsBitfield::kRoomTempFlag);
  data_ptr_[6] = temp.encoded_temp();
  data_ptr_[3] = temp.whole_degree() - kMinRoomTemp.whole_degree();
}

}  // namespace hackvac
