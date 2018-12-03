#ifndef CN105_PACKET_H_
#define CN105_PACKET_H_

#include <cstdint>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>

#include "hvac_settings.h"

#include "esp_cxx/cxx17hack.h"

namespace hackvac {

// Bit 6 seems to indicate if it is an ACK. So if 0x5a is a packet then
// 0x7a is its ack.
enum class PacketType : uint8_t {
  // TODO(awong): the second nibble might be zone info. a == zone 1, b = zone2, etc.
  kConnect = 0x5a,
  kConnectAck = 0x7a, 

  // TODO(awong): Handle 0x5b/0x7b.
  // https://github.com/SwiCago/HeatPump/issues/39#issuecomment-284234758
  //
  // Quoted from lekobob
  // here is the initial connect exchange:
  // fc,5a,01,30,02,ca,01,a8
  // fc,7a,01,30,01,00,54
  // fc,5b,01,30,01,c9,aa
  // fc,7b,01,30,10,c9,03,00,20,00,14,07,75,0c,05,a0,be,94,be,a0,be,a9
  kExtendedConnect = 0x5b,
  kExtendedConnectAck = 0x7b,

  // Update HVAC control status.
  kUpdate = 0x41, 
  kUpdateAck = 0x61, 

  // Requesting info from the HeatPump.
  kInfo = 0x42, 
  kInfoAck = 0x62, 

  kUnknown = 0x00,
};

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
//  TODO(awong): The 0x01 looks like variable header length and 0x30 is the
//               one byte in header. Maybe 0x30 means "should have checksum"
//
// Reference code:
//   https://github.com/hadleyrich/MQMitsi/blob/master/mitsi.py
class Cn105Packet {
  public:
    Cn105Packet();

    template <size_t n>
    Cn105Packet(PacketType type, std::array<uint8_t, n> data)
        : bytes_read_(kHeaderLength + data.size() + kChecksumSize) {
      static_assert(n < std::numeric_limits<uint8_t>::max() &&
                    n < kMaxPacketLength, "array too big");
      bytes_[kStartMarkerPos] = kPacketStartMarker;
      bytes_[kTypePos] = static_cast<uint8_t>(type);
      bytes_[2] = 0x01;  // TODO(ajwong): Make some constants.
      bytes_[3] = 0x30;
      bytes_[kDataLenPos] = data.size();
      if (n > 0) {
        std::copy(data.begin(), data.end(), bytes_.begin() + kDataStartPos);
      }
      bytes_[bytes_read_ - 1] =
          CalculateChecksum(&bytes_[0], kHeaderLength + data.size());
    }

    ~Cn105Packet();

    // Prints a packet to the ESP log stream.
    void DebugLog();

    // Simple thunk to call DebugLog() on the given packet.
    static void LogPacketThunk(std::unique_ptr<Cn105Packet> packet);

    ///
    /// Packet Building functions. Typically used when populating a default
    /// constructed packet byte-by-byte from an input stream.
    ///

    // Counts UART receive errors. This and IncrementUnexpectedEventCount()
    // update the last_error_ts.
    void IncrementErrorCount();

    // Counts unexpected UART events. This and IncrementErrorCount() update
    // the last_error_ts.
    void IncrementUnexpectedEventCount();

    // Appends a byte to the Cn105Packet.
    void AppendByte(uint8_t byte);

    // Returns true if the header has been read. After this,
    // packet type and data length can be read.
    bool IsHeaderComplete() const;

    // Returns true if the packet looks like junk. Specifically the start
    // marker is not kPacketStartMarker.
    bool IsJunk() const;

    // Returns true if current packet is complete.
    bool IsComplete() const;

    // Verifies the checksum on the packet.
    bool IsChecksumValid();

    // Calculates the CN105 protocol checksum for the given bytes. The
    // checksum algorithm, based on reverse-engineering packet captures
    // from CN105 to a PAC444CN, is checksum = (0xfc - sum(data)) & 0xff
    static uint8_t CalculateChecksum(const uint8_t* bytes, size_t size);

    // Returns number of bytes that should be read next.
    size_t NextChunkSize() const;

    // Header accessors. Returns valid data when IsHeaderComplete() is true.
    PacketType type() const { return static_cast<PacketType>(bytes_[kTypePos]); }
    uint8_t* data() { return &bytes_[kDataStartPos]; }
    size_t data_size() const { return bytes_[kDataLenPos]; }

    size_t packet_size() const { return kHeaderLength + data_size() + kChecksumSize; }
    const uint8_t* raw_bytes() const { return bytes_.data(); }
    size_t raw_bytes_size() const { return bytes_read_; }

    // Known value constants.
    static constexpr uint8_t kPacketStartMarker = 0xfc;

    // Size constants.
    static constexpr size_t kHeaderLength = 5;
    static constexpr size_t kChecksumSize = 1;

    // Packet field constants.
    static constexpr size_t kStartMarkerPos = 0;
    static constexpr size_t kTypePos = 1;
    static constexpr size_t kDataLenPos = 4;
    static constexpr size_t kDataStartPos = 5;

  private:
    // https://github.com/SwiCago/HeatPump assumes 22 byte max for full packet
    // but format-wise, data_len can be 255 so max packet size may be 261.
    // However that would be wasteful in memory usage so rouding up to 30.
    constexpr static size_t kMaxPacketLength = 30;

    size_t bytes_read_ = 0;
    std::array<uint8_t, kMaxPacketLength> bytes_;

    // Number of data-link layer errors.
    uint16_t error_count_ = 0;

    // Number of odd UART errors.
    uint16_t unexpected_event_count_ = 0;
    
    // Timestamp of when the first byte was received.
    uint32_t first_byte_ts = 0;

    // Timestamp of when the most recent byte was received.
    uint32_t last_byte_ts = 0;

    // Timestamp of when the most recent error was received.
    uint32_t last_error_ts = 0;
};

class ConnectPacket {
 public:
  static std::unique_ptr<Cn105Packet> Create() {
    static constexpr std::array<uint8_t, 2> data = { 0xca, 0x01 };
    return std::make_unique<Cn105Packet>(PacketType::kConnect, data);
  }
};

class ConnectAckPacket {
 public:
  static std::unique_ptr<Cn105Packet> Create() {
    static constexpr std::array<uint8_t, 1> data = { 0x00 };
    return std::make_unique<Cn105Packet>(PacketType::kConnectAck, data);
  }
};

class ExtendedConnectAckPacket {
 public:
  static std::unique_ptr<Cn105Packet> Create() {
    // TODO(awong): echo back the 0xc9 from the ExtendedConnectPacket.
    static constexpr std::array<uint8_t, 16> data = {
      0xc9, 0x03, 0x00, 0x20,
      0x00, 0x14, 0x07, 0x75,
      0x0c, 0x05, 0xa0, 0xbe,
      0x94, 0xbe, 0xa0, 0xbe };
    return std::make_unique<Cn105Packet>(PacketType::kExtendedConnectAck, data);
  }
};

// For UPDATE (0x41), the first data byte is the setting to change.
//   Control Update Extended (byte0)
//   0x07 = set current room temperature
//            byte0 = room temp 0x1, 
//            byte1 = ???
//            room temp is first byte after.
//            NOTE: MHK1 sends 0x80 for data if setting is nonsense (negative)
//                  yielding 0x00 on byte0 bitflag.
//
//   Control Update (byte0)
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

//  TODO(awong): Move this into an internal namespace.
enum class UpdateBitfield : uint8_t {
  kPowerFlag = 0x01,
  kModeFlag = 0x02,
  kTempFlag = 0x04,
  kFanFlag = 0x08,
  kVaneFlag = 0x10,
  kWideVaneFlag = 0x80,
};
enum class ExtendedUpdateBitfield : uint8_t {
  kRoomTempFlag = 0x01,
};
template <typename T> struct ExtractConfig;
template <> struct ExtractConfig<Power> {
  constexpr static UpdateBitfield kBitfield = UpdateBitfield::kPowerFlag;
  constexpr static int kDataPos = 3;
};
template <> struct ExtractConfig<Mode> {
  constexpr static UpdateBitfield kBitfield = UpdateBitfield::kModeFlag;
  constexpr static int kDataPos = 4;
};
template <> struct ExtractConfig<TargetTemp> {
  constexpr static UpdateBitfield kBitfield = UpdateBitfield::kTempFlag;
  constexpr static int kDataPos = 5;
};
template <> struct ExtractConfig<Fan> {
  constexpr static UpdateBitfield kBitfield = UpdateBitfield::kFanFlag;
  constexpr static int kDataPos = 6;
};
template <> struct ExtractConfig<Vane> {
  constexpr static UpdateBitfield kBitfield = UpdateBitfield::kVaneFlag;
  constexpr static int kDataPos = 7;
};
template <> struct ExtractConfig<WideVane> {
  constexpr static UpdateBitfield kBitfield = UpdateBitfield::kWideVaneFlag;
  constexpr static int kDataPos = 10;
};

// TODO(awong): Handle WideVane.
class UpdatePacket {
 public:
  explicit UpdatePacket(Cn105Packet* packet) : packet_(packet) {}
  static std::unique_ptr<Cn105Packet> Create(const HvacSettings& settings) {
    return {};
  };
  static std::unique_ptr<Cn105Packet> Create(const ExtendedSettings& extended_settings) {
    return {};
  };

  template <typename T>
  std::optional<T> GetSetting() {
    if (!HasField(ExtractConfig<T>::kBitfield)) {
      return {};
    }
    return static_cast<T>(packet_->data()[ExtractConfig<T>::kDataPos]);
  }
  template <typename T>
  void SetSetting(T value) {
    SetField(ExtractConfig<T>::kBitfield);
    packet_->data()[ExtractConfig<T>::kDataPos] = static_cast<uint8_t>(value);
  }

  void ApplyUpdate(HvacSettings* settings) {
    if (auto power = GetSetting<Power>()) settings->power = power.value();
    if (auto mode = GetSetting<Mode>()) settings->mode = mode.value();
    if (auto target_temp = GetSetting<TargetTemp>()) settings->target_temp = target_temp.value();
    if (auto fan = GetSetting<Fan>()) settings->fan = fan.value();
    if (auto vane = GetSetting<Vane>()) settings->vane = vane.value();
    if (auto wide_vane = GetSetting<WideVane>()) settings->wide_vane = wide_vane.value();
  }

 private:
  UpdateType update_type() { return static_cast<UpdateType>(packet_->data()[0]); }

  // Field accessors.
  // TODO(ajwong): widevane support on byte 2.
  bool HasField(UpdateBitfield field) {
    return update_type() == UpdateType::kNormalSettings &&
      packet_->data()[1] & static_cast<uint8_t>(field);
  }
  void SetField(UpdateBitfield field) {
    packet_->data()[0] = static_cast<uint8_t>(UpdateType::kNormalSettings);
    packet_->data()[1] |= static_cast<uint8_t>(field);
  }
  bool HasField(ExtendedUpdateBitfield field) {
    return update_type() == UpdateType::kExtendedSettings &&
      packet_->data()[1] & static_cast<uint8_t>(field);
  }
  void SetField(ExtendedUpdateBitfield field) {
    packet_->data()[0] = static_cast<uint8_t>(UpdateType::kExtendedSettings);
    packet_->data()[1] |= static_cast<uint8_t>(field);
  }

  // Not owned.
  Cn105Packet* packet_;
};

// This Ack seems to always send 16-bytes of 0-data. Doing it just for
// kicks I guess.
class UpdateAckPacket {
 public:
  explicit UpdateAckPacket(Cn105Packet* packet) : packet_(packet) {}

  static std::unique_ptr<Cn105Packet> Create() {
    static constexpr std::array<uint8_t, 16> kBlank16BytePacket = {};
    return std::make_unique<Cn105Packet>(PacketType::kUpdateAck, kBlank16BytePacket);
  }

  UpdateType type() const { return static_cast<UpdateType>(packet_->data()[0]); }

  bool IsValid() const {
    return packet_ && !packet_->IsJunk() && packet_->IsComplete() && packet_->IsChecksumValid();
  }

 private:
  Cn105Packet *packet_;
};

class InfoPacket {
 public:
  // TODO(awong): Assert every packet wrapper takes only well formed packets.
  explicit InfoPacket(Cn105Packet* packet) : packet_(packet) {}

  static std::unique_ptr<Cn105Packet> Create(InfoType type) {
    // TODO(awong): constructor for initializing X blank bytes.
    static constexpr std::array<uint8_t, 16> kBlank16BytePacket = {};
    auto packet = std::make_unique<Cn105Packet>(PacketType::kUpdateAck, kBlank16BytePacket);
    packet->data()[0] = static_cast<uint8_t>(type);
    return std::move(packet);
  }

  InfoType type() const { return static_cast<InfoType>(packet_->data()[0]); }

 private:
  Cn105Packet *packet_;
};

class InfoAckPacket {
 public:
  explicit InfoAckPacket(Cn105Packet* packet) : packet_(packet) {}

  static std::unique_ptr<Cn105Packet> Create(const HvacSettings& settings) {
    std::array<uint8_t, 0x10> setting_data = {};
    setting_data.at(0) = static_cast<uint8_t>(InfoType::kSettings);
    // TODO(awong): Make the HvacSettings struct internal representation this.
    //  It's the "settings" data format.
    setting_data.at(3) = static_cast<uint8_t>(settings.power);
    setting_data.at(4) = static_cast<uint8_t>(settings.mode);
    setting_data.at(5) = static_cast<uint8_t>(settings.target_temp);
    setting_data.at(6) = static_cast<uint8_t>(settings.fan);
    setting_data.at(7) = static_cast<uint8_t>(settings.vane);
    setting_data.at(10) = static_cast<uint8_t>(settings.wide_vane);
    return std::make_unique<Cn105Packet>(PacketType::kInfoAck, setting_data);
  }

  InfoType type() const { return static_cast<InfoType>(packet_->data()[0]); }

  bool IsValid() const {
    return packet_ && !packet_->IsJunk() && packet_->IsComplete() && packet_->IsChecksumValid();
  }

  std::optional<HvacSettings> settings() const {
    if (type() != InfoType::kSettings) {
      return {};
    }

    // TODO(awong): I assume bitfields 1 and 2 are the mirror of the update packet
    // for what settings have been returned.
    HvacSettings settings;
    settings.power = static_cast<Power>(packet_->data()[3]);
    bool hasISee = packet_->data()[4] > 0x08;  // Affects mode field.
    settings.mode = static_cast<Mode>(packet_->data()[4] - (hasISee ? 0x08 : 0));
    settings.target_temp = static_cast<TargetTemp>(packet_->data()[5]);
    settings.fan = static_cast<Fan>(packet_->data()[6]);
    settings.vane = static_cast<Vane>(packet_->data()[7]);
    settings.wide_vane = static_cast<WideVane>(packet_->data()[10]);

    // Handle higher resoluation temperature field. This overrides data()[5].
    if (packet_->data()[11]) {
      // TODO(awong): This loses 0.5C of resolution. Fix.
      uint8_t high_res_target_temp = (packet_->data()[11] - 128) / 2;
      settings.target_temp = static_cast<TargetTemp>(high_res_target_temp);
    }

    return settings;
  }

  std::optional<ExtendedSettings> extended_settings() const {
    return {};
  }

 private:
  Cn105Packet* packet_;
};

}  // namespace hackvac

#endif  // CN105_PACKET_H_

