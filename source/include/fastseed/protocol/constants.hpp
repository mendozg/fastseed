#pragma once
#include <array>
#include <cstdint>
namespace fastseed::bsty {
inline constexpr std::array<std::uint8_t,4> MAINNET_MAGIC{0xA2,0xB2,0xE2,0xF2};
inline constexpr std::uint16_t DEFAULT_P2P_PORT = 8226;
inline constexpr std::int32_t PROTOCOL_VERSION = 70016;
inline constexpr std::int32_t INITIAL_VERSION = 209;
inline constexpr std::int32_t MIN_PEER_VERSION = 31800;
inline constexpr std::size_t HEADER_SIZE = 24;
inline constexpr std::size_t MAX_MESSAGE_SIZE = 4 * 1024 * 1024;
}
