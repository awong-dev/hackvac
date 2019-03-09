#include "../cn105_packet.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace hackvac {

TEST(Cn105Packet, Default) {
  Cn105Packet packet;
  EXPECT_FALSE(packet.IsJunk());
  EXPECT_FALSE(packet.IsHeaderComplete());
  EXPECT_FALSE(packet.IsComplete());
  EXPECT_FALSE(packet.IsChecksumValid());
}

TEST(Cn105Packet, IsJunk) {
  Cn105Packet packet;
  EXPECT_FALSE(packet.IsJunk());
  EXPECT_FALSE(packet.IsHeaderComplete());
  EXPECT_FALSE(packet.IsComplete());
  EXPECT_FALSE(packet.IsChecksumValid());
  EXPECT_EQ(Cn105Packet::kHeaderLength, packet.NextChunkSize());

  // A good byte doesn't flip the Junk flag.
  packet.AppendByte(Cn105Packet::kPacketStartMarker);
  EXPECT_FALSE(packet.IsJunk());

  // A bad byte does flip the flag but leaves all completion markers false.
  packet = {};

  packet.AppendByte(Cn105Packet::kPacketStartMarker - 1);
  EXPECT_TRUE(packet.IsJunk());
  EXPECT_EQ(Cn105Packet::kMaxPacketLength - 1, packet.NextChunkSize());

  EXPECT_FALSE(packet.IsHeaderComplete());
  EXPECT_FALSE(packet.IsComplete());
  EXPECT_FALSE(packet.IsChecksumValid());
}

TEST(Cn105Packet, PacketParsing) {
  Cn105Packet packet;
  // Validate initial state of accessors and completness markers.
  EXPECT_FALSE(packet.IsHeaderComplete());
  EXPECT_FALSE(packet.IsComplete());
  EXPECT_EQ(Cn105Packet::kHeaderLength, packet.NextChunkSize());
  EXPECT_EQ(0, packet.raw_bytes_size());
  EXPECT_TRUE(packet.raw_bytes());
  EXPECT_TRUE(packet.raw_bytes_str().empty());

  // Start marker doesn't make it complete.
  packet.AppendByte(Cn105Packet::kPacketStartMarker);
  EXPECT_FALSE(packet.IsHeaderComplete());
  EXPECT_FALSE(packet.IsComplete());
  EXPECT_EQ(1, packet.raw_bytes_size());
  EXPECT_EQ(Cn105Packet::kPacketStartMarker, static_cast<uint8_t>(packet.raw_bytes_str().at(0)));
  EXPECT_EQ(Cn105Packet::kHeaderLength - 1, packet.NextChunkSize());

  // Fill out most of header excluding the last byte.
  packet.AppendByte(static_cast<uint8_t>(PacketType::kConnect));  // type.
  packet.AppendByte(0x01);  // Unknown but constant field 1.
  packet.AppendByte(0x30);  // Unknown but constant field 2.
  EXPECT_FALSE(packet.IsHeaderComplete());
  EXPECT_FALSE(packet.IsComplete());
  EXPECT_EQ(1, packet.NextChunkSize());  // One byte left in header.

  static constexpr int kDataSize = 3;
  packet.AppendByte(kDataSize);
  EXPECT_TRUE(packet.IsHeaderComplete());
  EXPECT_FALSE(packet.IsComplete());
  EXPECT_EQ(PacketType::kConnect, packet.type());
  EXPECT_EQ(kDataSize, packet.data_size());
  EXPECT_EQ(kDataSize + 1, packet.NextChunkSize());  // Data + checksum.

  // Insert data bytes.
  EXPECT_EQ(0, packet.data()[0]); // Ensure bytes are initialized to 0.
  EXPECT_EQ(0, packet.data()[1]);
  EXPECT_EQ(0, packet.data()[2]);

  packet.AppendByte(0xcc);
  EXPECT_EQ(0xcc, packet.data()[0]);

  packet.AppendByte(0xdd);
  EXPECT_EQ(0xdd, packet.data()[1]);

  packet.AppendByte(0xee);
  EXPECT_EQ(0xee, packet.data()[2]);

  EXPECT_TRUE(packet.IsHeaderComplete());
  EXPECT_FALSE(packet.IsComplete());

  // Insert checksum byte.
  packet.AppendByte(Cn105Packet::CalculateChecksum(packet.raw_bytes(), packet.packet_size() - 1));
  EXPECT_TRUE(packet.IsHeaderComplete());
  EXPECT_TRUE(packet.IsChecksumValid());
  EXPECT_EQ(0, packet.NextChunkSize());
}

}  // namespace hackvac
