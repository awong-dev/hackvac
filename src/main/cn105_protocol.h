#ifndef PACKET_FACTORY_H_
#define PACKET_FACTORY_H_

#include "cn105_packet.h"

// Set of classes that front Cn105Packet to parse or create specific packet
// types.

namespace hackvac {

static constexpr std::array<uint8_t, 16> kBlank16BytePacket = {};

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
//   0x01 = set a bunch of settings.
//   0x07 = set current room temperature
class UpdatePacket {
 public:
  explicit UpdatePacket(Cn105Packet* packet) : packet_(packet) {}

  static std::unique_ptr<Cn105Packet> Create(const StoredHvacSettings& settings) {
    auto packet = std::make_unique<Cn105Packet>(PacketType::kUpdate, settings.encoded_bytes());
    packet->data()[0] = static_cast<uint8_t>(InfoType::kSetSettings);
    return std::move(packet);
  };

  static std::unique_ptr<Cn105Packet> Create(const ExtendedSettings& extended_settings) {
    auto packet = std::make_unique<Cn105Packet>(PacketType::kUpdate, extended_settings.encoded_bytes());
    packet->data()[0] = static_cast<uint8_t>(InfoType::kSetExtendedSettings);
    return std::move(packet);
  };

  void ApplyUpdate(StoredHvacSettings* settings) {
    HvacSettings received_settings(packet_->data());
    // TODO(awong): copy/assignment needs to be fixed for StoredHvacSettings!
    settings->MergeUpdate(received_settings);
  }

 private:
  // Not owned.
  Cn105Packet* packet_;
};

// This Ack seems to always send 16-bytes of 0-data. Doing it just for
// kicks I guess.
class UpdateAckPacket {
 public:
  explicit UpdateAckPacket(Cn105Packet* packet) : packet_(packet) {}

  static std::unique_ptr<Cn105Packet> Create() {
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
    auto packet = std::make_unique<Cn105Packet>(PacketType::kInfo, kBlank16BytePacket);
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

  static std::unique_ptr<Cn105Packet> Create(const StoredHvacSettings& settings) {
    auto packet = std::make_unique<Cn105Packet>(PacketType::kInfoAck, settings.encoded_bytes());
    packet->data()[0] = static_cast<uint8_t>(InfoType::kSettings);
    return std::move(packet);
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
    return HvacSettings(packet_->data());
  }

  std::optional<ExtendedSettings> extended_settings() const {
    return {};
  }

 private:
  Cn105Packet* packet_;
};

}  // namespace hackvac

#endif  // PACKET_FACTORY_H_