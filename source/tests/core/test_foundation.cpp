#include "fastseed/core/constants.hpp"
#include "fastseed/core/error.hpp"
#include "fastseed/core/version.hpp"
#include <iostream>
#include <limits>
#include <type_traits>

using namespace fastseed::core;

static_assert(!std::is_default_constructible_v<Port>);
static_assert(!std::is_constructible_v<Port, int>);
static_assert(!std::is_convertible_v<Port, std::uint16_t>);
static_assert(!std::is_convertible_v<std::uint64_t, Sequence>);
static_assert(!std::is_constructible_v<Sequence, UnixSeconds>);
static_assert(!std::is_assignable_v<Sequence&, UnixSeconds>);
static_assert(!std::is_constructible_v<PeerId, SeederId>);
static_assert(!std::is_assignable_v<PeerId&, SeederId>);
static_assert(Port::from(8226)->value() == 8226);
static_assert(bsty_protocol_version == 70016);
static_assert(bsty_min_peer_version == 31800);
static_assert(!mesh_v2_available && !production_release);

int main() {
    unsigned checks = 0;
    auto check = [&](bool condition, const char* message) {
        ++checks;
        if (!condition) { std::cerr << "FAIL: " << message << '\n'; return false; }
        return true;
    };
    bool ok = true;
    ok &= check(!Port::from(0), "reject unspecified endpoint port");
    ok &= check(Port::from(1)->value() == 1, "minimum port");
    ok &= check(Port::from(65535)->value() == 65535, "maximum port");
    ok &= check(!Port::from(65536), "reject before uint16 truncation");
    ok &= check(!Port::from(std::numeric_limits<std::uint64_t>::max()), "reject largest integer");
    ok &= check(*Port::from(1) < *Port::from(8226), "port ordering");
    ok &= check(Sequence{9} < Sequence{10}, "sequence ordering");
    ok &= check(UnixSeconds{10}.value() == 10, "explicit timestamp units");
    ok &= check(Sequence{std::numeric_limits<std::uint64_t>::max()}.value() ==
                    std::numeric_limits<std::uint64_t>::max(), "preserve full counter width");
    PeerId first{}, second{};
    second.bytes[31] = 1;
    ok &= check(first != second, "identity compares all 32 bytes");
    ok &= check(second == PeerId{second.bytes}, "identity value round trip");
    ok &= check(error_message(static_cast<ErrorCode>(255)) == "unknown error", "unknown error is bounded");
    ok &= check(error_message(ErrorCode::unsupported_version) == "unsupported version", "version diagnostic");
    ok &= check(bsty_p2p_port.value() == 8226 && bsty_magic[0] == 0xA2 && bsty_magic[3] == 0xF2,
                "BSTY network constants preserved");
    ok &= check(version == "2.0.0-alpha2" && version_identity.find("Development") != std::string_view::npos,
                "development identity");
    if (!ok) return 1;
    std::cout << "Core foundation: " << checks << " checks PASS\n";
}
