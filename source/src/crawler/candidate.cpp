#include "fastseed/crawler/candidate.hpp"
#include "fastseed/protocol/addrv2.hpp"
#include "fastseed/protocol/wire_messages.hpp"
#include <boost/asio/ip/address.hpp>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace fastseed::crawler {
std::string_view to_string(CandidateState s) noexcept { switch(s){case CandidateState::Queued:return "queued";case CandidateState::Probing:return "probing";case CandidateState::Validated:return "validated";case CandidateState::Failed:return "failed";case CandidateState::Cooldown:return "cooldown";case CandidateState::AwaitingTransport:return "awaiting_transport";} return "unknown"; }
std::string_view to_string(CandidateSource s) noexcept { switch(s){case CandidateSource::Bootstrap:return "bootstrap";case CandidateSource::Addr:return "addr";case CandidateSource::AddrV2:return "addrv2";case CandidateSource::PersistentDatabase:return "persistent_database";case CandidateSource::Manual:return "manual";case CandidateSource::PeerExchange:return "peer_exchange";} return "unknown"; }
std::string_view to_string(CandidateNetwork n) noexcept { switch(n){case CandidateNetwork::IPv4:return "ipv4";case CandidateNetwork::IPv6:return "ipv6";case CandidateNetwork::TorV3:return "torv3";case CandidateNetwork::I2P:return "i2p";case CandidateNetwork::Unknown:return "unknown";} return "unknown"; }

static std::string hex(std::span<const std::uint8_t> bytes) {
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (auto b : bytes) out << std::setw(2) << static_cast<unsigned>(b);
    return out.str();
}

std::string PeerCandidate::key() const {
    if(auto value=endpoint()) return value->legacy_key();
    if (network == CandidateNetwork::IPv4 || network == CandidateNetwork::IPv6 || network == CandidateNetwork::Unknown)
        return host + ":" + std::to_string(port);
    return std::string(to_string(network)) + ":" + host + ":" + std::to_string(port);
}

std::optional<peer::Endpoint> PeerCandidate::endpoint() const {
    const auto checked=core::Port::from(port);if(!checked)return std::nullopt;
    try {
        if(!address_bytes.empty() && network!=CandidateNetwork::Unknown) {
            auto value=peer::Endpoint::from_bytes(static_cast<peer::Network>(network),address_bytes,*checked);
            if((network==CandidateNetwork::IPv4 || network==CandidateNetwork::IPv6) && !host.empty() && peer::Endpoint::parse(host,*checked)!=value)return std::nullopt;
            return value; // Overlay transports use the binary key, not its display label.
        }
        auto value=peer::Endpoint::parse(host,*checked);
        if(!address_bytes.empty() && !std::equal(address_bytes.begin(),address_bytes.end(),value.address().begin(),value.address().end()))return std::nullopt;
        if(network!=CandidateNetwork::Unknown && static_cast<unsigned>(network)!=static_cast<unsigned>(value.network()) &&
           !(network==CandidateNetwork::IPv6 && value.network()==peer::Network::IPv4 && host.find(':')!=host.npos))return std::nullopt;
        return value;
    } catch(const std::invalid_argument&) {return std::nullopt;}
}
PeerCandidate PeerCandidate::canonicalized() const {
    auto out=*this;
    if(auto value=endpoint()) {out.host=value->host();out.network=static_cast<CandidateNetwork>(value->network());out.address_bytes.assign(value->address().begin(),value->address().end());}
    return out;
}

PeerCandidate make_addrv2_candidate(const protocol::AddrV2Address& a, CandidateSource source) {
    protocol::validate_addrv2_address(a.network_id, a.address);
    PeerCandidate c;
    c.port = a.port;
    c.address_bytes = a.address;
    c.source = source;
    switch (a.network_id) {
    case 0x01: {
        if (a.address.size()!=4) throw std::invalid_argument("invalid IPv4 ADDRv2 address");
        boost::asio::ip::address_v4::bytes_type b{}; std::copy(a.address.begin(),a.address.end(),b.begin());
        c.network=CandidateNetwork::IPv4; c.host=boost::asio::ip::address_v4(b).to_string(); break;
    }
    case 0x02: {
        if (a.address.size()!=16) throw std::invalid_argument("invalid IPv6 ADDRv2 address");
        boost::asio::ip::address_v6::bytes_type b{}; std::copy(a.address.begin(),a.address.end(),b.begin());
        c.network=CandidateNetwork::IPv6; c.host=boost::asio::ip::address_v6(b).to_string(); break;
    }
    case 0x04: c.network=CandidateNetwork::TorV3; c.host="onion:"+hex(a.address); c.state=CandidateState::AwaitingTransport; break;
    case 0x05: c.network=CandidateNetwork::I2P; c.host="i2p:"+hex(a.address); c.state=CandidateState::AwaitingTransport; break;
    default: c.network=CandidateNetwork::Unknown; c.host="net"+std::to_string(a.network_id)+":"+hex(a.address); c.state=CandidateState::AwaitingTransport; break;
    }
    return c.canonicalized();
}
} // namespace fastseed::crawler
