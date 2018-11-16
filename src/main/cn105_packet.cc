#include "cn105_packet.h"

#include <esp_log.h>

namespace hackvac {
constexpr std::array<uint8_t, 16> Cn105Packet::kBlank16BytePacket;

Cn105Packet::Cn105Packet() = default;
Cn105Packet::~Cn105Packet() = default;

bool Cn105Packet::IsHeaderComplete() const {
  return bytes_read_ >= kHeaderLength;
}

bool Cn105Packet::IsJunk() const {
  return (bytes_read_ > 0) && (bytes_[0] != kPacketStartMarker);
}

size_t Cn105Packet::NextChunkSize() const {
  if (IsJunk()) {
    return bytes_.size() - bytes_read_;
  }

  if (!IsHeaderComplete())  {
    return kHeaderLength - bytes_read_;
  }
  return kHeaderLength + data_size() + kChecksumSize - bytes_read_;
}

bool Cn105Packet::IsComplete() const {
  if (IsJunk()) {
    return bytes_read_ < bytes_.size();
  }

  if (!IsHeaderComplete()) {
    return false;
  }
  return (bytes_read_ >= packet_size());
}

bool Cn105Packet::IsChecksumValid() {
  // TODO(ajwong): Ensure no off-edge reads!
  return CalculateChecksum(bytes_.data(), packet_size() - 1) ==
    bytes_.at(packet_size() - 1);
}

uint8_t Cn105Packet::CalculateChecksum(const uint8_t* bytes, size_t size) {
  uint32_t checksum = 0xfc;
  for (size_t i = 0; i < size; ++i) {
    checksum -= bytes[i];
  }
  return checksum;
}

void Cn105Packet::IncrementErrorCount() {
  last_error_ts = esp_log_timestamp();
  error_count_++;
}

void Cn105Packet::IncrementUnexpectedEventCount() {
  last_error_ts = esp_log_timestamp();
  unexpected_event_count_++;
}

void Cn105Packet::AppendByte(uint8_t byte) {
  last_byte_ts = esp_log_timestamp();
  if (bytes_read_ == 0) {
    first_byte_ts = last_byte_ts;
  }

  bytes_.at(bytes_read_++) = byte;
}

////
//// Packet Factories.
////

void LogPacket(const char* tag, PacketDirection dir,
               std::unique_ptr<Cn105Packet> packet) {
  // TODO(awong): Shift to using a queue.
  ESP_LOGI(tag, "%s %d bytes", dir == PacketDirection::kTx ? "tx" : "rx", packet->packet_size());
  ESP_LOG_BUFFER_HEX_LEVEL(tag, packet->raw_bytes(),
                           packet->raw_bytes_size(),
                           ESP_LOG_INFO);
  // TODO(awong): Print timestamp.
  if (packet->IsJunk() ||
      !packet->IsComplete() ||
      !packet->IsChecksumValid()) {
    ESP_LOGI(tag, "Bad packet. junk: %d complete %d expected checksum %x actual %x",
             packet->IsJunk(),
             packet->IsComplete(),
             packet->CalculateChecksum(
                 packet->raw_bytes(), packet->packet_size() - 1),
             packet->raw_bytes()[packet->packet_size() - 1]);
    return;
  }
}

}  // namespace hackvac
