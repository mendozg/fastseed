#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/message.hpp"
#include "fastseed/protocol/parser.hpp"
#include "fastseed/protocol/protocol_error.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include "fastseed/protocol/version_message.hpp"
#include <boost/asio/ip/address.hpp>
#include <cassert>
#include <iostream>
#include <vector>
using namespace fastseed;
static std::vector<std::uint8_t> raw_message(const std::string& cmd,std::vector<std::uint8_t> payload={}) { protocol::MessageHeader h;h.magic=bsty::MAINNET_MAGIC;std::copy(cmd.begin(),cmd.end(),h.command.begin());h.payload_size=payload.size();h.checksum=protocol::checksum(payload);auto x=protocol::encode_header(h);x.insert(x.end(),payload.begin(),payload.end());return x; }
int main(){
 using namespace protocol;
 assert((bsty::MAINNET_MAGIC==std::array<std::uint8_t,4>{0xA2,0xB2,0xE2,0xF2}));
 for(const auto& text: {"127.0.0.1","255.255.255.255","::1","2001:db8::1"}) {auto a=boost::asio::ip::make_address(text);NetworkAddress n{0,a,8226};auto v=encode_version(make_version(n,n,123));auto d=decode_version(v);assert(d.addr_recv.address==a);assert(d.addr_recv.port==8226);assert(d.start_height==123);}
 auto good=raw_message("verack"); Parser p; auto m=p.parse(good); assert(command_string(m.header)=="verack" && m.payload.empty());
 auto bad=good;bad[0]^=1;try{p.parse(bad);assert(false);}catch(const ProtocolError&e){assert(e.code()==ProtocolErrorCode::InvalidMagic);}
 bad=good;bad[20]^=1;try{p.parse(bad);assert(false);}catch(const ProtocolError&e){assert(e.code()==ProtocolErrorCode::InvalidChecksum);}
 bad=good;bad[16]=0xff;bad[17]=0xff;bad[18]=0xff;bad[19]=0xff;try{p.parse(bad);assert(false);}catch(const ProtocolError&e){assert(e.code()==ProtocolErrorCode::MessageTooLarge);}
 bad.resize(10);try{p.parse(bad);assert(false);}catch(const ProtocolError&e){assert(e.code()==ProtocolErrorCode::Truncated);}
 std::vector<std::uint8_t> version_payload; NetworkAddress n{0,boost::asio::ip::make_address("::1"),8226}; auto vm=encode_version(make_version(n,n)); assert(vm.size()<protocol::limits::max_version_payload);
 std::cout<<"Phase 1.1 protocol hardening tests: PASS\n";
}
