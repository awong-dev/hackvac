#include "cn105_packet.h"

namespace hackvac {

Cn105Packet::Cn105Packet() = default;
Cn105Packet::~Cn105Packet() = default;

bool Cn105Packet::IsHeaderComplete() const {
  return bytes_read_ >= kHeaderLength;
}

size_t Cn105Packet::NextChunkSize() const {
  if (!IsHeaderComplete())  {
    return kHeaderLength - bytes_read_;
  }
  return kHeaderLength + data_size() + kChecksumSize - bytes_read_;
}

bool Cn105Packet::IsComplete() const {
  if (!IsHeaderComplete()) {
    return false;
  }
  return (bytes_read_ >= packet_size());
}

bool Cn105Packet::IsChecksumValid() {
  return CalculateChecksum(bytes_.data(), bytes_read_ - 1) ==
    bytes_.at(bytes_read_);
}

uint8_t Cn105Packet::CalculateChecksum(uint8_t* bytes, size_t size) {
  uint32_t checksum = 0xfc;
  for (size_t i = 0; i < size; ++i) {
    checksum -= bytes[i];
  }
  return checksum;
}

////
//// Packet Factories.
////

}  // namespace hackvac
