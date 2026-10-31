#include "fastseed/protocol/message.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/protocol_error.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include "fastseed/protocol/parser.hpp"
#include <algorithm>
namespace fastseed::protocol {
void validate_command(const std::array<char,12>& command) {
 if(command[0]=='\0') throw ProtocolError(ProtocolErrorCode::InvalidCommand,"empty command");
 bool padding=false;
 for(unsigned char c:command) {
  if(c==0) padding=true;
  else if(padding || c<0x20 || c>0x7e) throw ProtocolError(ProtocolErrorCode::InvalidCommand,"invalid command bytes or padding");
 }
}
std::vector<std::uint8_t> encode_header(const MessageHeader& h){
 validate_command(h.command);
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
 validate_command(h.command);
 return h;
}
std::string command_string(const MessageHeader& h){auto n=std::find(h.command.begin(),h.command.end(),'\0');return {h.command.begin(),n};}
std::vector<std::uint8_t> encode_message(std::string_view command,
                                          std::span<const std::uint8_t> payload) {
    if (command.empty() || command.size() > limits::max_command_size)
        throw ProtocolError(ProtocolErrorCode::InvalidCommand, "invalid command length");
    if (std::find(command.begin(), command.end(), '\0') != command.end())
        throw ProtocolError(ProtocolErrorCode::InvalidCommand, "command contains NUL");
    if (payload.size() > limits::max_message_size)
        throw ProtocolError(ProtocolErrorCode::MessageTooLarge, "message too large");

    MessageHeader h;
    h.magic = bsty::MAINNET_MAGIC;
    std::copy(command.begin(), command.end(), h.command.begin());
    h.payload_size = static_cast<std::uint32_t>(payload.size());
    validate_command(h.command);
    h.checksum = checksum(payload);

    auto wire = encode_header(h);
    wire.insert(wire.end(), payload.begin(), payload.end());
    return wire;
}

Message decode_message(std::span<const std::uint8_t> wire, bool require_mainnet_magic) {
    Parser parser(require_mainnet_magic);
    return parser.parse(wire);
}

}
