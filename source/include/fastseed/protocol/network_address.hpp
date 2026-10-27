#pragma once
#include <utility>
#include <array>
#include <boost/asio/ip/address.hpp>
#include <cstdint>
namespace fastseed::protocol {
struct NetworkAddress { std::uint64_t services{}; boost::asio::ip::address address; std::uint16_t port{}; };
std::array<std::uint8_t,16> ipv4_mapped(const boost::asio::ip::address& address);
void write_network_address(class Writer& w, const NetworkAddress& a);
NetworkAddress read_network_address(class Reader& r);
}
