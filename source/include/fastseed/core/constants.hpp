#pragma once

#include "fastseed/core/types.hpp"
#include "fastseed/protocol/constants.hpp"
#include <string_view>

namespace fastseed::core {
inline constexpr std::string_view bsty_network = "mainnet";
inline constexpr Port bsty_p2p_port = *Port::from(bsty::DEFAULT_P2P_PORT);
inline constexpr auto bsty_magic = bsty::MAINNET_MAGIC;
// These describe the existing BSTY P2P protocol, not a FastSeed mesh version.
inline constexpr auto bsty_protocol_version = bsty::PROTOCOL_VERSION;
inline constexpr auto bsty_min_peer_version = bsty::MIN_PEER_VERSION;

// Planning metadata only: alpha1 does not implement or negotiate Mesh v2.
inline constexpr std::uint16_t planned_mesh_version = 2;
inline constexpr bool mesh_v2_available = false;
} // namespace fastseed::core
