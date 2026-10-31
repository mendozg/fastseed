#include "fastseed/protocol/parser.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include <algorithm>
namespace fastseed::protocol {
Message Parser::parse(std::span<const std::uint8_t> bytes) const {
 if(bytes.size()<bsty::HEADER_SIZE) throw ProtocolError(ProtocolErrorCode::Truncated,"truncated message header");
 auto h=decode_header(bytes.first(bsty::HEADER_SIZE));
 if(require_mainnet_magic_ && h.magic!=bsty::MAINNET_MAGIC) throw ProtocolError(ProtocolErrorCode::InvalidMagic,"invalid BSTY magic");
 if(h.payload_size>limits::max_message_size) throw ProtocolError(ProtocolErrorCode::MessageTooLarge,"message exceeds limit");
 const auto expected_size = bsty::HEADER_SIZE + static_cast<std::size_t>(h.payload_size);
 if(bytes.size()<expected_size) throw ProtocolError(ProtocolErrorCode::Truncated,"message length mismatch");
 if(bytes.size()>expected_size) throw ProtocolError(ProtocolErrorCode::TrailingData,"trailing message bytes");
 auto payload=bytes.subspan(bsty::HEADER_SIZE,h.payload_size);
 if(checksum(payload)!=h.checksum) throw ProtocolError(ProtocolErrorCode::InvalidChecksum,"invalid checksum");
 return Message{h,std::vector<std::uint8_t>(payload.begin(),payload.end())};
}
}
