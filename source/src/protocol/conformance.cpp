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

std::string_view canonical_command(MessageType type) noexcept {
    switch (type) {
    case MessageType::Version: return "version";
    case MessageType::Verack: return "verack";
    case MessageType::SendAddrV2: return "sendaddrv2";
    case MessageType::WtxidRelay: return "wtxidrelay";
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
