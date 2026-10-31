#include "fastseed/protocol/wire_messages.hpp"
#include "fastseed/protocol/version_message.hpp"
#include "fastseed/protocol/conformance.hpp"
#include "fastseed/protocol/serializer.hpp"
#include "fastseed/core/version.hpp"
#include "known_vectors.hpp"
#include "test_support.hpp"
#include <boost/asio/ip/address.hpp>
using namespace fastseed::protocol;
using namespace test_support;
int main() {
    Suite t;
    NetworkAddress recv{1,boost::asio::ip::make_address("203.0.113.10"),8226};
    NetworkAddress from{0,boost::asio::ip::make_address("2001:db8::1"),8226};
    VersionMessage v;v.version=70016;v.services=1;v.timestamp=1700000000;
    v.addr_recv=recv;v.addr_from=from;v.nonce=0x1122334455667788ULL;v.user_agent="/fixture/";v.start_height=42;v.relay=false;
    auto wire=hex(vectors::version_payload);
    t.check(encode_version(v)==wire,"independent version fixture");
    auto decoded=decode_version(wire);
    t.check(decoded.version==70016 && decoded.services==1 && decoded.timestamp==1700000000 && decoded.user_agent=="/fixture/" && decoded.start_height==42 && !decoded.relay,"version fields");
    t.check(decoded.addr_recv.address==recv.address && decoded.addr_recv.port==8226 && decoded.addr_from.address==from.address,"network address byte order");
    t.check(make_version(recv,from).user_agent=="/BSTYFastSeed:"+std::string(fastseed::core::version)+"/","advertised application version");
    wire.pop_back();t.check(decode_version(wire).relay,"optional relay preserved");
    wire.push_back(2);wire.push_back(0xa5);t.check(decode_version(wire).relay,"nonzero relay and bounded future extensions preserved");
    auto noncanonical=hex(vectors::version_payload);noncanonical[80]=253;noncanonical.insert(noncanonical.begin()+81,{9,0});
    t.error(ProtocolErrorCode::NonCanonical,[&]{decode_version(noncanonical);});
    v.user_agent=std::string(256,'a');t.check(decode_version(encode_version(v)).user_agent==v.user_agent,"maximum user agent");
    v.user_agent.push_back('a');t.error(ProtocolErrorCode::MessageTooLarge,[&]{encode_version(v);});
    auto oversized=hex(vectors::version_payload);oversized[80]=253;oversized.insert(oversized.begin()+81,{1,1});
    t.error(ProtocolErrorCode::MessageTooLarge,[&]{decode_version(oversized);});
    oversized.resize(limits::max_version_payload+1);t.error(ProtocolErrorCode::MessageTooLarge,[&]{decode_version(oversized);});
    wire=hex(vectors::version_payload);
    for(std::size_t n=0;n<wire.size()-1;++n) t.error(ProtocolErrorCode::Truncated,[&]{decode_version(std::span<const std::uint8_t>(wire).first(n));});
    const std::vector<PeerAddress> peers={{1700000000,recv}};
    t.check(encode_addr(peers)==hex(vectors::addr_payload),"independent addr fixture");
    auto addresses=decode_addr(hex(vectors::addr_payload));t.check(addresses.size()==1 && addresses[0].address.port==8226 && addresses[0].address.address==recv.address,"addr decoded fixture");
    AddrV2Address a{1700000000,1,1,{203,0,113,10},8226};
    AddrV2Address b{1700000001,0,2,hex("20010db8000000000000000000000001"),8226};
    t.check(encode_addrv2({a,b})==hex(vectors::addrv2_payload),"independent addrv2 fixture");
    auto av=decode_addrv2(hex(vectors::addrv2_payload));t.check(av.size()==2 && av[1].address==b.address && av[0].port==8226,"addrv2 decoded fixture");
    a.services=UINT64_MAX;t.check(decode_addrv2(encode_addrv2({a}))[0].services==UINT64_MAX,"full width service bits");
    for(auto id:{4,5}) {a.network_id=static_cast<std::uint8_t>(id);a.address=std::vector<std::uint8_t>(32,0x42);t.check(decode_addrv2(encode_addrv2({a}))[0].address==a.address,"overlay address bytes retained (no live connection)");}
    a.network_id=0x42;a.address={1,2,3};t.check(decode_addrv2(encode_addrv2({a}))[0].network_id==0x42,"unknown network bytes retained");
    for(bool v2:{false,true}) {
        auto bytes=hex(v2?vectors::addrv2_payload:vectors::addr_payload);
        for(std::size_t n=0;n<bytes.size();++n) {
            auto input=std::span<const std::uint8_t>(bytes).first(n);
            t.error(ProtocolErrorCode::Truncated,[&]{if(v2) decode_addrv2(input);else decode_addr(input);});
        }
        const auto excessive=hex("fde903");t.error(ProtocolErrorCode::MessageTooLarge,[&]{if(v2) decode_addrv2(excessive);else decode_addr(excessive);});
        bytes.push_back(0);t.error(ProtocolErrorCode::TrailingData,[&]{if(v2) decode_addrv2(bytes);else decode_addr(bytes);});
    }
    for(std::size_t n=0;n<=9;++n) if(n!=8) {
        std::vector<std::uint8_t> malformed(n);
        t.error(ProtocolErrorCode::InvalidPayload,[&]{decode_ping_nonce(malformed);});
        t.error(ProtocolErrorCode::InvalidPayload,[&]{decode_pong_nonce(malformed);});
    }
    t.check(encode_ping(0x1122334455667788ULL)==hex(vectors::ping_payload) && decode_pong_nonce(hex(vectors::pong_payload))==0x1122334455667788ULL,"nonce fixture");
    t.check(message_type("wtxidrelay")==MessageType::WtxidRelay && canonical_command(MessageType::WtxidRelay)=="wtxidrelay" && encode_wtxidrelay().empty(),"wtxidrelay capability codec");
    t.pass("Alpha2 payload codecs");
}
