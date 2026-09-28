#pragma once
#include "message.hpp"
#include "protocol_error.hpp"
#include <span>
namespace fastseed::protocol {
class Parser {
public:
 explicit Parser(bool require_mainnet_magic = true) : require_mainnet_magic_(require_mainnet_magic) {}
 Message parse(std::span<const std::uint8_t> bytes) const;
private: bool require_mainnet_magic_;
};
}
