#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>

namespace fastseed::protocol { struct AddrV2Address; }

namespace fastseed::crawler {

enum class CandidateState { Queued, Probing, Validated, Failed, Cooldown, AwaitingTransport };
enum class CandidateSource { Bootstrap, Addr, AddrV2, PersistentDatabase, Manual, PeerExchange };
enum class CandidateNetwork : std::uint8_t { IPv4=0x01, IPv6=0x02, TorV3=0x04, I2P=0x05, Unknown=0xff };

std::string_view to_string(CandidateState state) noexcept;
std::string_view to_string(CandidateSource source) noexcept;
std::string_view to_string(CandidateNetwork network) noexcept;

struct PeerCandidate {
    std::string host;
    std::uint16_t port{0};
    CandidateSource source{CandidateSource::Manual};
    CandidateNetwork network{CandidateNetwork::Unknown};
    std::vector<std::uint8_t> address_bytes;
    CandidateState state{CandidateState::Queued};
    std::uint32_t attempts{0};
    std::uint32_t successes{0};
    std::uint32_t failures{0};
    std::string last_error;

    bool transport_probe_supported() const noexcept {
        return network == CandidateNetwork::IPv4 || network == CandidateNetwork::IPv6;
    }
    std::string key() const;
};

PeerCandidate make_addrv2_candidate(const protocol::AddrV2Address& address,
                                    CandidateSource source = CandidateSource::AddrV2);

} // namespace fastseed::crawler
