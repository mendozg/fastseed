#pragma once
#include "fastseed/core/types.hpp"
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fastseed::peer {
enum class Network : std::uint8_t { IPv4=1, IPv6=2, TorV3=4, I2P=5 };

// A canonical endpoint, not an authenticated remote identity. DNS names must be
// resolved before construction. Only the four supported binary address families.
class Endpoint {
public:
    static Endpoint from_bytes(Network network, std::span<const std::uint8_t> address, core::Port port);
    static Endpoint parse(std::string_view host, core::Port port);
    Network network() const noexcept { return network_; }
    core::Port port() const noexcept { return port_; }
    std::span<const std::uint8_t> address() const noexcept;
    std::string host() const;     // Existing internal onion:/i2p: hex presentation.
    std::string hostname() const; // Validated .onion / ordinary .b32.i2p presentation.
    std::string legacy_key() const;
    std::vector<std::uint8_t> canonical_bytes() const;
    core::PeerId id() const;
    auto operator<=>(const Endpoint&) const = default;
private:
    Endpoint(Network network, core::Port port) : network_(network), port_(port) {}
    Network network_;
    core::Port port_;
    std::array<std::uint8_t,32> address_{};
};
std::string id_hex(const core::PeerId& id);
// Supports existing host:port and torv3:onion:hex:port / i2p:i2p:hex:port keys.
std::optional<Endpoint> endpoint_from_key(std::string_view key);
}
