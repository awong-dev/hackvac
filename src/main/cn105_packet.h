#ifndef CN105_PACKET_H_
#define CN105_PACKET_H_

#include <cstdint>

#include <array>

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
// Reference code:
//   https://github.com/hadleyrich/MQMitsi/blob/master/mitsi.py
class Cn105Packet {
  public:
    // Bit 6 seems to indicate if it is an ACK. So if 0x5a is a packet then
    // 0x7a is its ack.
    enum class PacketType : uint8_t {
      // TODO(awong): the second nibble might be zone info. a == zone 1, b = zone2, etc.
      kConnect = 0x5a,
      kConnectAck = 0x7a, 

      // Update HVAC control status.
      kUpdate = 0x41, 
      kUpdateAck = 0x61, 

      // Requesting info from the HeatPump.
      kInfo = 0x42, 
      kInfoAck = 0x62, 

      kUnknown = 0x00,
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

    // Reset the packet to uninitialized state.
    void Reset() {
      bytes_read_ = 0;
    }

    // Returns true if byte succeeded. This can fail if the packet is complete,
    // or if the the packet length was exceeded.
    bool AppendByte(uint8_t byte) {
      if (IsComplete() || bytes_read_ >= bytes_.size()) {
        return false;
      }
      if (bytes_read_ >= bytes_.size()) {
        return false;
      }
      bytes_.at(bytes_read_++) = byte;
      return true;
    }

    // Size constants.
    static constexpr size_t kHeaderLength = 5;
    static constexpr size_t kChecksumSize = 1;

    // Packet field constants.
    static constexpr size_t kStartMarkerPos = 0;
    static constexpr size_t kTypePos = 1;
    static constexpr size_t kDataLenPos = 4;

    // Returns true if the header has been read. After this,
    // packet type and data length can be read.
    bool IsHeaderComplete() const {
      return bytes_read_ >= kHeaderLength;
    }

    size_t NextChunkSize() {
      if (!IsHeaderComplete())  {
        return kHeaderLength - bytes_read_;
      }
      return kHeaderLength + data_size() + kChecksumSize - bytes_read_;
    }

    // Header accessors. Returns valid data when IsHeaderComplete() is true.
    size_t data_size() const { return bytes_[kDataLenPos]; }
    PacketType type() const { return static_cast<PacketType>(bytes_[kTypePos]); }
    size_t packet_size() const { return kHeaderLength + data_size() + kChecksumSize; }

    // Returns true if current packet is complete.
    bool IsComplete() const {
      if (!IsHeaderComplete()) {
        return false;
      }
      return (bytes_read_ >= packet_size());
    }

    // Verifies the checksum on the packet. The checksum algorithm, based on
    // reverse-engineering packet captures from CN105 to a PAC444CN, is:
    // checksum = (0xfc - sum(data)) & 0xff
    bool IsChecksumValid() {
      uint32_t checksum = 0xfc;
      for (size_t i = 0; i < (bytes_read_ - 1); ++i) {
        checksum -= bytes_.at(i);
      }

      return checksum == bytes_.at(bytes_read_);
    }

    // https://github.com/SwiCago/HeatPump assumes 22 byte max for full packet
    // but format-wise, data_len can be 255 so max packet size may be 261.
    // However that would be wasteful in memory usage so rouding up to 30.
    constexpr static size_t kMaxPacketLength = 30;

  private:
    std::array<uint8_t, kMaxPacketLength> bytes_;
    size_t bytes_read_;
};

}  // namespace hackvac

#endif  // CN105_PACKET_H_

