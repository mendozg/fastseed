#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <optional>

namespace fastseed::core {

// Endpoint ports exclude zero. Listener port zero (ephemeral binding) remains
// a separate concern in the existing server configuration.
class Port final {
public:
    [[nodiscard]] static constexpr std::optional<Port> from(std::uint64_t value) noexcept {
        if (value == 0 || value > 65535) return std::nullopt;
        return Port(static_cast<std::uint16_t>(value));
    }
    [[nodiscard]] constexpr std::uint16_t value() const noexcept { return value_; }
    auto operator<=>(const Port&) const = default;
private:
    explicit constexpr Port(std::uint16_t value) noexcept : value_(value) {}
    std::uint16_t value_;
};

// Values have no implicit integer conversion or arithmetic. In particular,
// increment/overflow and wall-clock freshness policies belong to their users.
template<class Tag>
class Counter final {
public:
    explicit constexpr Counter(std::uint64_t value = 0) noexcept : value_(value) {}
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    auto operator<=>(const Counter&) const = default;
private:
    std::uint64_t value_;
};
struct SequenceTag {};
struct UnixSecondsTag {};
using Sequence = Counter<SequenceTag>;
// Seconds since Unix epoch; never use this type for monotonic timeouts.
using UnixSeconds = Counter<UnixSecondsTag>;

template<class Tag>
struct Identifier final {
    std::array<std::uint8_t, 32> bytes{};
    auto operator<=>(const Identifier&) const = default;
};
struct PeerIdTag {};
struct SeederIdTag {};
using PeerId = Identifier<PeerIdTag>;
using SeederId = Identifier<SeederIdTag>;
// IDs are storage types only. Construction is not identity authentication.
// Canonical PeerId derivation and signed SeederId verification are later stages.

} // namespace fastseed::core
