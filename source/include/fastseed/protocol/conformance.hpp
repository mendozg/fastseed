#pragma once

#include "message.hpp"
#include "wire_messages.hpp"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace fastseed::protocol {

enum class HandshakeState {
    Disconnected,
    Connecting,
    VersionSent,
    VersionReceived,
    VerackSent,
    Established,
    Failed
};

enum class HandshakeEvent {
    BeginConnect,
    VersionSent,
    VersionReceived,
    VerackSent,
    VerackReceived,
    Failure
};

class HandshakeStateMachine {
public:
    HandshakeState state() const noexcept { return state_; }
    void reset() noexcept { state_ = HandshakeState::Disconnected; }
    bool transition(HandshakeEvent event) noexcept;

private:
    HandshakeState state_{HandshakeState::Disconnected};
};

std::vector<std::uint8_t> encode_message(std::string_view command,
                                          std::span<const std::uint8_t> payload);
Message decode_message(std::span<const std::uint8_t> wire,
                       bool require_mainnet_magic = true);

std::string_view canonical_command(MessageType type) noexcept;

void require_pong_nonce(std::uint64_t expected,
                        std::span<const std::uint8_t> payload);

} // namespace fastseed::protocol
