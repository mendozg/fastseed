#include "fastseed/peer/endpoint.hpp"
#include "fastseed/crawler/candidate.hpp"
#include "../protocol/test_support.hpp"
#include "known_peer_vectors.hpp"
using namespace fastseed;
using peer::Endpoint;using peer::Network;
int main() {
    test_support::Suite t;const auto port=*core::Port::from(8226);
    auto reject=[&](auto fn,const char* label="invalid endpoint rejected"){bool failed=false;try{fn();}catch(const std::invalid_argument&){failed=true;}t.check(failed,label);};
    auto ip4=Endpoint::parse("203.0.113.7",port),ip6=Endpoint::parse("2001:DB8:0000:0000:0000:0000:0000:0001",port);
    std::vector<std::uint8_t> key(32);for(unsigned i=0;i<32;++i)key[i]=static_cast<std::uint8_t>(i);
    auto tor=Endpoint::from_bytes(Network::TorV3,key,port),i2p=Endpoint::from_bytes(Network::I2P,key,port);
    struct Vector {Endpoint endpoint;std::string_view bytes,id;};
    Vector vectors[]={{ip4,peer_vectors::ipv4_bytes,peer_vectors::ipv4_id},{ip6,peer_vectors::ipv6_bytes,peer_vectors::ipv6_id},
        {tor,peer_vectors::tor_bytes,peer_vectors::tor_id},{i2p,peer_vectors::i2p_bytes,peer_vectors::i2p_id},
        {Endpoint::parse("203.0.113.7",*core::Port::from(8227)),peer_vectors::other_port_bytes,peer_vectors::other_port_id}};
    for(const auto& v:vectors) {
        t.check(v.endpoint.canonical_bytes()==test_support::hex(v.bytes),"independent canonical bytes");
        t.check(peer::id_hex(v.endpoint.id())==v.id,"independent SHA256 PeerId");
        t.check(Endpoint::parse(v.endpoint.host(),v.endpoint.port())==v.endpoint,"internal host roundtrip");
        t.check(Endpoint::parse(v.endpoint.hostname(),v.endpoint.port())==v.endpoint,"public hostname roundtrip");
        t.check(peer::endpoint_from_key(v.endpoint.legacy_key())==v.endpoint,"legacy key bridge");
    }
    t.check(ip6==Endpoint::parse("[2001:db8::1]",port),"bracketed/compressed IPv6 alias");
    t.check(ip4==Endpoint::parse("::ffff:203.0.113.7",port),"IPv4-mapped normalization");
    t.check(ip4.id()!=ip6.id() && tor.id()!=i2p.id() && ip4.id()!=vectors[4].endpoint.id(),"family/port identity separation");
    t.check(tor.hostname()==peer_vectors::tor_host && i2p.hostname()==peer_vectors::i2p_host,"independent overlay encodings");
    std::string upper(tor.hostname());for(char& c:upper)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');
    t.check(Endpoint::parse(upper,port)==tor,"case-insensitive onion hostname");
    auto bad=tor.hostname();bad[3]=bad[3]=='a'?'b':'a';reject([&]{Endpoint::parse(bad,port);});
    bad=i2p.hostname();bad[51]='b';reject([&]{Endpoint::parse(bad,port);});
    for(const char* host:{"", "seed.example", "fe80::1%eth0", "[203.0.113.7]", "onion:00", "i2p:zz", "abc.onion", "abc.b32.i2p", "127.1", "127.00.0.1", "::ffff:127.1", "1.2.3.256", "1.2.3.4.5"})reject([&]{Endpoint::parse(host,port);},host);
    reject([&]{Endpoint::from_bytes(Network::IPv4,key,port);});reject([&]{Endpoint::from_bytes(static_cast<Network>(6),key,port);});
    for(const char* text:{"203.0.113.7:0","203.0.113.7:65536","203.0.113.7:8226x","203.0.113.7:-1","seed.example:8226"})t.check(!peer::endpoint_from_key(text),"invalid legacy key");
    crawler::PeerCandidate c;c.host="203.0.113.7";c.port=8226;c.network=crawler::CandidateNetwork::IPv4;c.address_bytes={203,0,113,8};
    t.check(!c.endpoint(),"conflicting IP text/binary rejected");c.address_bytes={203,0,113,7};t.check(c.endpoint()==ip4,"consistent IP candidate");
    c.network=crawler::CandidateNetwork::Unknown;c.address_bytes={203,0,113,8};t.check(!c.endpoint(),"inferred IP family still checks binary address");
    c.host="overlay display label";c.network=crawler::CandidateNetwork::TorV3;c.address_bytes=key;
    t.check(c.endpoint()==tor && c.canonicalized().host==tor.host(),"overlay binary key is authoritative");
    t.pass("Alpha3 peer identity");
}
