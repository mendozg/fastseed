#include "fastseed/protocol/version_message.hpp"
#include "fastseed/protocol/constants.hpp"
#include "fastseed/protocol/serializer.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include <boost/asio/ip/address_v4.hpp>
#include <chrono>
#include <stdexcept>
#include <bit>
#include "fastseed/core/version.hpp"
#include "fastseed/protocol/protocol_error.hpp"
namespace fastseed::protocol {
std::array<std::uint8_t,16> ipv4_mapped(const boost::asio::ip::address& a){std::array<std::uint8_t,16> x{};auto b=a.to_v4().to_bytes();x[10]=0xff;x[11]=0xff;std::copy(b.begin(),b.end(),x.begin()+12);return x;}
void write_network_address(Writer& w,const NetworkAddress&a){w.u64(a.services);std::array<std::uint8_t,16>x{};if(a.address.is_v4())x=ipv4_mapped(a.address);else if(a.address.is_v6())x=a.address.to_v6().to_bytes();else throw std::invalid_argument("invalid address");w.fixed16(x); // port is network byte order
 w.be16(a.port);}
NetworkAddress read_network_address(Reader&r){NetworkAddress a;a.services=r.u64();auto x=r.fixed16();a.port=r.be16();bool mapped=true;for(int i=0;i<10;++i)if(x[i]!=0)mapped=false;if(x[10]!=0xff||x[11]!=0xff)mapped=false;if(mapped){boost::asio::ip::address_v4::bytes_type b{};std::copy_n(x.begin()+12,4,b.begin());a.address=boost::asio::ip::address_v4(b);}else{boost::asio::ip::address_v6::bytes_type b{};std::copy(x.begin(),x.end(),b.begin());a.address=boost::asio::ip::address_v6(b);}return a;}
VersionMessage make_version(const NetworkAddress&recv,const NetworkAddress&from,std::int32_t height){VersionMessage v;v.version=bsty::PROTOCOL_VERSION;v.services=0;v.timestamp=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();v.addr_recv=recv;v.addr_from=from;v.nonce=std::chrono::steady_clock::now().time_since_epoch().count();v.user_agent="/BSTYFastSeed:"+std::string(core::version)+"/";v.start_height=height;v.relay=true;return v;}
std::vector<std::uint8_t> encode_version(const VersionMessage& v) {
    if (v.user_agent.size() > limits::max_user_agent_size)
        throw ProtocolError(ProtocolErrorCode::MessageTooLarge, "user agent exceeds limit");
    Writer w(limits::max_version_payload);
    w.i32(v.version); w.u64(v.services); w.u64(static_cast<std::uint64_t>(v.timestamp));
    write_network_address(w, v.addr_recv); write_network_address(w, v.addr_from);
    w.u64(v.nonce); w.string(v.user_agent); w.i32(v.start_height); w.u8(v.relay ? 1 : 0);
    return w.data();
}
VersionMessage decode_version(std::span<const std::uint8_t> b) {
    if (b.size() > limits::max_version_payload)
        throw ProtocolError(ProtocolErrorCode::MessageTooLarge, "version payload exceeds limit");
    Reader r(b); VersionMessage v;
    v.version = r.i32(); v.services = r.u64(); v.timestamp = std::bit_cast<std::int64_t>(r.u64());
    v.addr_recv = read_network_address(r); v.addr_from = read_network_address(r);
    v.nonce = r.u64(); v.user_agent = r.varstring(limits::max_user_agent_size);
    v.start_height = r.i32();
    // Preserve the existing optional relay field and nonzero=true semantics.
    // Unknown trailing version extensions remain accepted within the size cap.
    if (r.remaining()) v.relay = r.u8() != 0;
    return v;
}
}
