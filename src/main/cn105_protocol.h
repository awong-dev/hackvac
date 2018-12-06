#ifndef PACKET_FACTORY_H_
#define PACKET_FACTORY_H_

#include "cn105_packet.h"

// Set of classes that front Cn105Packet to parse or create specific packet
// types comprising the CN105 protocol.
//
// Cn105 is a series of command/ack packet pairs sent from the client to the
// HVAC controller. The main pairs are
//
// Connect/ConnectAck,
// ExtendedConnect/ExtendedConnectAck,
// Update/UpdateAck  (This is should be called Set)
// Info/InfoAck  (This is should be called Get)
//
// The Update and InfoAck packets send and receive setting information. The
// structure of the data bytes in both seem to be the same. Even the data
// byte that indicates the action (SetSettings =0x01, GetSettings=0x02)
// seem to avoid colliding in value even though the values can be sorted
// into two sets which can only be used sensibly with either Update or
// InfoAck.

namespace hackvac {

// TODO(ajwong): Get rid of this and move it into the raw packet.
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
    packet->data()[0] = static_cast<uint8_t>(CommandType::kSetSettings);
    return std::move(packet);
  };

  static std::unique_ptr<Cn105Packet> Create(const StoredExtendedSettings& extended_settings) {
    auto packet = std::make_unique<Cn105Packet>(PacketType::kUpdate, extended_settings.encoded_bytes());
    packet->data()[0] = static_cast<uint8_t>(CommandType::kSetExtendedSettings);
    return std::move(packet);
  };

  HvacSettings settings() { return HvacSettings(packet_->data()); }

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

  CommandType type() const { return static_cast<CommandType>(packet_->data()[0]); }

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

  static std::unique_ptr<Cn105Packet> Create(CommandType type) {
    // TODO(awong): constructor for initializing X blank bytes.
    auto packet = std::make_unique<Cn105Packet>(PacketType::kInfo, kBlank16BytePacket);
    packet->data()[0] = static_cast<uint8_t>(type);
    return std::move(packet);
  }

  CommandType type() const { return static_cast<CommandType>(packet_->data()[0]); }

 private:
  Cn105Packet *packet_;
};

class InfoAckPacket {
 public:
  explicit InfoAckPacket(Cn105Packet* packet) : packet_(packet) {}

  static std::unique_ptr<Cn105Packet> Create(const StoredHvacSettings& settings) {
    auto packet = std::make_unique<Cn105Packet>(PacketType::kInfoAck, settings.encoded_bytes());
    packet->data()[0] = static_cast<uint8_t>(CommandType::kSettings);
    return std::move(packet);
  }

  CommandType type() const { return static_cast<CommandType>(packet_->data()[0]); }

  bool IsValid() const {
    return packet_ && !packet_->IsJunk() && packet_->IsComplete() && packet_->IsChecksumValid();
  }

  std::optional<HvacSettings> settings() const {
    if (type() != CommandType::kSettings) {
      return {};
    }

    return HvacSettings(packet_->data());
  }

  std::optional<ExtendedSettings> extended_settings() const {
    if (type() != CommandType::kExtendedSettings) {
      return {};
    }

    return ExtendedSettings(packet_->data());
  }

 private:
  Cn105Packet* packet_;
};

}  // namespace hackvac

#endif  // PACKET_FACTORY_H_
