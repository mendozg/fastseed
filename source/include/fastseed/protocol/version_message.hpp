#pragma once
#include "network_address.hpp"
#include <span>
#include <vector>
#include <cstdint>
#include <string>
namespace fastseed::protocol {
struct VersionMessage {
 std::int32_t version{};
 std::uint64_t services{};
 std::int64_t timestamp{};
 NetworkAddress addr_recv;
 NetworkAddress addr_from;
 std::uint64_t nonce{};
 std::string user_agent;
 std::int32_t start_height{};
 bool relay{true};
};
VersionMessage make_version(const NetworkAddress& recv, const NetworkAddress& from, std::int32_t start_height = 0);
std::vector<std::uint8_t> encode_version(const VersionMessage& v);
VersionMessage decode_version(std::span<const std::uint8_t> bytes);
}
