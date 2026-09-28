#include "fastseed/protocol/message.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/protocol_error.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include <algorithm>
namespace fastseed::protocol {
std::vector<std::uint8_t> encode_header(const MessageHeader& h){
 if(h.payload_size>limits::max_message_size) throw ProtocolError(ProtocolErrorCode::MessageTooLarge,"message too large");
 std::vector<std::uint8_t> o; o.reserve(bsty::HEADER_SIZE); o.insert(o.end(),h.magic.begin(),h.magic.end()); o.insert(o.end(),h.command.begin(),h.command.end());
 for(int i=0;i<4;++i) o.push_back(static_cast<std::uint8_t>(h.payload_size>>(8*i)));
 o.insert(o.end(),h.checksum.begin(),h.checksum.end());
 return o;
}
MessageHeader decode_header(std::span<const std::uint8_t> b){
 if(b.size()!=bsty::HEADER_SIZE) throw ProtocolError(ProtocolErrorCode::InvalidHeader,"header size");
 MessageHeader h; std::copy_n(b.begin(),4,h.magic.begin()); std::copy_n(b.begin()+4,12,h.command.begin());
 h.payload_size=static_cast<std::uint32_t>(b[16])|(static_cast<std::uint32_t>(b[17])<<8)|(static_cast<std::uint32_t>(b[18])<<16)|(static_cast<std::uint32_t>(b[19])<<24); std::copy_n(b.begin()+20,4,h.checksum.begin());
 if(h.payload_size>limits::max_message_size) throw ProtocolError(ProtocolErrorCode::MessageTooLarge,"message too large");
 return h;
}
std::string command_string(const MessageHeader& h){auto n=std::find(h.command.begin(),h.command.end(),'\0');return {h.command.begin(),n};}
}
