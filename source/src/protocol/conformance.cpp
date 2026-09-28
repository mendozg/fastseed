#include "fastseed/protocol/conformance.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/parser.hpp"
#include "fastseed/protocol/protocol_error.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace fastseed::protocol {

bool HandshakeStateMachine::transition(HandshakeEvent event) noexcept {
    switch (state_) {
    case HandshakeState::Disconnected:
        if (event == HandshakeEvent::BeginConnect) { state_ = HandshakeState::Connecting; return true; }
        break;
    case HandshakeState::Connecting:
        if (event == HandshakeEvent::VersionSent) { state_ = HandshakeState::VersionSent; return true; }
        break;
    case HandshakeState::VersionSent:
        if (event == HandshakeEvent::VersionReceived) { state_ = HandshakeState::VersionReceived; return true; }
        break;
    case HandshakeState::VersionReceived:
        if (event == HandshakeEvent::VerackSent) { state_ = HandshakeState::VerackSent; return true; }
        break;
    case HandshakeState::VerackSent:
        if (event == HandshakeEvent::VerackReceived) { state_ = HandshakeState::Established; return true; }
        break;
    case HandshakeState::Established:
        break;
    case HandshakeState::Failed:
        break;
    }
    if (event == HandshakeEvent::Failure) { state_ = HandshakeState::Failed; return true; }
    return false;
}

std::vector<std::uint8_t> encode_message(std::string_view command,
                                          std::span<const std::uint8_t> payload) {
    if (command.empty() || command.size() > limits::max_command_size)
        throw ProtocolError(ProtocolErrorCode::InvalidCommand, "invalid command length");
    if (std::find(command.begin(), command.end(), '\0') != command.end())
        throw ProtocolError(ProtocolErrorCode::InvalidCommand, "command contains NUL");
    if (payload.size() > limits::max_message_size)
        throw ProtocolError(ProtocolErrorCode::MessageTooLarge, "message too large");

    MessageHeader h;
    h.magic = bsty::MAINNET_MAGIC;
    std::copy(command.begin(), command.end(), h.command.begin());
    h.payload_size = static_cast<std::uint32_t>(payload.size());
    h.checksum = checksum(payload);

    auto wire = encode_header(h);
    wire.insert(wire.end(), payload.begin(), payload.end());
    return wire;
}

Message decode_message(std::span<const std::uint8_t> wire, bool require_mainnet_magic) {
    Parser parser(require_mainnet_magic);
    return parser.parse(wire);
}

std::string_view canonical_command(MessageType type) noexcept {
    switch (type) {
    case MessageType::Version: return "version";
    case MessageType::Verack: return "verack";
    case MessageType::SendAddrV2: return "sendaddrv2";
    case MessageType::Addr: return "addr";
    case MessageType::AddrV2: return "addrv2";
    case MessageType::GetAddr: return "getaddr";
    case MessageType::Ping: return "ping";
    case MessageType::Pong: return "pong";
    case MessageType::Unknown: return {};
    }
    return {};
}

void require_pong_nonce(std::uint64_t expected,
                        std::span<const std::uint8_t> payload) {
    if (decode_pong_nonce(payload) != expected)
        throw ProtocolError(ProtocolErrorCode::InvalidVersion, "pong nonce mismatch");
}

} // namespace fastseed::protocol
