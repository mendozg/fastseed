#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/message.hpp"
#include "fastseed/protocol/version_message.hpp"
#include <boost/asio/ip/address.hpp>
#include <cassert>
#include <iostream>
int main(){using namespace fastseed;assert(bsty::MAINNET_MAGIC[0]==0xA2);assert(bsty::DEFAULT_P2P_PORT==8226);protocol::NetworkAddress a{0,boost::asio::ip::make_address("127.0.0.1"),8226};protocol::NetworkAddress b{0,boost::asio::ip::make_address("::1"),8226};auto v=protocol::make_version(a,b);auto enc=protocol::encode_version(v);auto dec=protocol::decode_version(enc);assert(dec.version==bsty::PROTOCOL_VERSION);assert(dec.addr_recv.address==a.address);assert(dec.addr_recv.port==8226);assert(dec.addr_from.address==b.address);assert(dec.user_agent==v.user_agent);auto cs=protocol::checksum(enc);protocol::MessageHeader h;h.magic=bsty::MAINNET_MAGIC;std::string c="version";std::copy(c.begin(),c.end(),h.command.begin());h.payload_size=enc.size();h.checksum=cs;auto raw=protocol::encode_header(h);assert(raw.size()==24);auto h2=protocol::decode_header(raw);assert(h2.payload_size==enc.size());assert(protocol::command_string(h2)=="version");std::cout<<"Phase 1 protocol tests: PASS\n";}
