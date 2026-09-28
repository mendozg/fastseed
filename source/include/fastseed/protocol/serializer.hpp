#pragma once
#include <cstdint>
#include <span>
#include <array>
#include <string>
#include <vector>
namespace fastseed::protocol {
class Writer {
public:
 void u8(std::uint8_t v); void i32(std::int32_t v); void u32(std::uint32_t v); void u64(std::uint64_t v);
 void bytes(std::span<const std::uint8_t> v); void string(const std::string& s); void fixed16(std::span<const std::uint8_t> v);
 const std::vector<std::uint8_t>& data() const noexcept { return data_; }
private: std::vector<std::uint8_t> data_;
};
class Reader {
public:
 explicit Reader(std::span<const std::uint8_t> data) : data_(data) {}
 std::uint8_t u8(); std::int32_t i32(); std::uint32_t u32(); std::uint64_t u64();
 std::vector<std::uint8_t> bytes(std::size_t n); std::string string(std::size_t n); std::array<std::uint8_t,16> fixed16();
 std::size_t remaining() const noexcept { return data_.size()-pos_; }
private: std::span<const std::uint8_t> data_; std::size_t pos_{0}; void need(std::size_t n) const;
};
}
