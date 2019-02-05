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
    Cn105Packet(PacketType type, const std::array<uint8_t, n>& data)
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

    // Returns true if the packet looks like junk. Specifically the start
    // marker is not kPacketStartMarker.
    bool IsJunk() const;

    // Returns true if the header has been read. After this,
    // packet type and data length can be read.
    bool IsHeaderComplete() const;

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

}  // namespace hackvac

#endif  // CN105_PACKET_H_

