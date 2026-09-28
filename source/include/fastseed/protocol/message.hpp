#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace fastseed::protocol {
struct MessageHeader {
    std::array<std::uint8_t,4> magic{};
    std::array<char,12> command{};
    std::uint32_t payload_size{};
    std::array<std::uint8_t,4> checksum{};
};
struct Message { MessageHeader header{}; std::vector<std::uint8_t> payload; };
std::array<std::uint8_t,4> checksum(std::span<const std::uint8_t> payload);
std::vector<std::uint8_t> encode_header(const MessageHeader& h);
MessageHeader decode_header(std::span<const std::uint8_t> bytes);
std::string command_string(const MessageHeader& h);
}
