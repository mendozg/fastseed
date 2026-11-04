#include "fastseed/peer/endpoint.hpp"
#include "fastseed/protocol/constants.hpp"
#include <boost/asio/ip/address.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace fastseed::peer {
namespace {
std::size_t width(Network network) {
    switch(network) { case Network::IPv4:return 4; case Network::IPv6:return 16; case Network::TorV3:case Network::I2P:return 32; }
    throw std::invalid_argument("unsupported endpoint network");
}
std::array<std::uint8_t,4> ipv4_bytes(std::string_view text) {
    std::array<std::uint8_t,4> out{};
    for(std::size_t i=0;i<4;++i) {
        const auto end=text.find('.');const auto part=text.substr(0,end);
        unsigned value=0;const auto parsed=std::from_chars(part.data(),part.data()+part.size(),value);
        if(part.empty() || (part.size()>1 && part.front()=='0') || parsed.ec!=std::errc{} || parsed.ptr!=part.data()+part.size() || value>255 || (i<3 && end==text.npos) || (i==3 && end!=text.npos))
            throw std::invalid_argument("IPv4 requires four canonical decimal octets");
        out[i]=static_cast<std::uint8_t>(value);
        if(i<3)text.remove_prefix(end+1);
    }
    return out;
}
std::string hex(std::span<const std::uint8_t> bytes) {
    constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(bytes.size()*2);
    for(auto b:bytes) {out.push_back(digits[b>>4]);out.push_back(digits[b&15]);}return out;
}
std::vector<std::uint8_t> unhex(std::string_view text) {
    if(text.size()!=64) throw std::invalid_argument("overlay key must be 32 bytes");
    auto digit=[](char c)->unsigned {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("invalid hex key");};
    std::vector<std::uint8_t> out;out.reserve(32);
    for(std::size_t i=0;i<text.size();i+=2)out.push_back(static_cast<std::uint8_t>((digit(text[i])<<4)|digit(text[i+1])));
    return out;
}
std::array<std::uint8_t,32> digest(std::span<const std::uint8_t> bytes,const EVP_MD* algorithm) {
    std::array<std::uint8_t,32> out{};unsigned n=0;
    if(!algorithm || EVP_Digest(bytes.data(),bytes.size(),out.data(),&n,algorithm,nullptr)!=1 || n!=32)
        throw std::runtime_error("endpoint digest failed");
    return out;
}
std::string base32(std::span<const std::uint8_t> data) {
    constexpr char alphabet[]="abcdefghijklmnopqrstuvwxyz234567";std::string out;unsigned bits=0,buffer=0;
    for(auto b:data) {buffer=(buffer<<8)|b;bits+=8;while(bits>=5){bits-=5;out.push_back(alphabet[(buffer>>bits)&31]);}}
    if(bits)out.push_back(alphabet[(buffer<<(5-bits))&31]);return out;
}
std::vector<std::uint8_t> unbase32(std::string_view text,std::size_t expected) {
    std::vector<std::uint8_t> out;unsigned bits=0,buffer=0;
    for(char c:text) {
        unsigned n;
        if(c>='a'&&c<='z')n=c-'a';else if(c>='2'&&c<='7')n=c-'2'+26;else throw std::invalid_argument("invalid base32 address");
        buffer=(buffer<<5)|n;bits+=5;if(bits>=8){bits-=8;out.push_back(static_cast<std::uint8_t>(buffer>>bits));}
    }
    if(out.size()!=expected || (bits && (buffer&((1u<<bits)-1u))))throw std::invalid_argument("noncanonical base32 address");
    return out;
}
std::array<std::uint8_t,32> onion_checksum(std::span<const std::uint8_t> key) {
    constexpr std::string_view prefix=".onion checksum";
    std::vector<std::uint8_t> input(prefix.begin(),prefix.end());input.insert(input.end(),key.begin(),key.end());input.push_back(3);
    return digest(input,EVP_sha3_256());
}
}
std::span<const std::uint8_t> Endpoint::address() const noexcept {
    return {address_.data(),network_==Network::IPv4?4u:network_==Network::IPv6?16u:32u};
}
Endpoint Endpoint::from_bytes(Network network,std::span<const std::uint8_t> address,core::Port port) {
    if(address.size()!=width(network))throw std::invalid_argument("endpoint address length");
    if(network==Network::IPv6 && std::all_of(address.begin(),address.begin()+10,[](auto b){return b==0;}) && address[10]==255 && address[11]==255)
        return from_bytes(Network::IPv4,address.subspan(12),port);
    Endpoint out(network,port);std::copy(address.begin(),address.end(),out.address_.begin());return out;
}
Endpoint Endpoint::parse(std::string_view input,core::Port port) {
    if(input.empty() || input.size()>255 || input.find('\0')!=input.npos || input.find('%')!=input.npos)
        throw std::invalid_argument("invalid or scoped endpoint");
    std::string host(input);for(auto& c:host)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    if(host.starts_with("onion:"))return from_bytes(Network::TorV3,unhex(std::string_view(host).substr(6)),port);
    if(host.starts_with("i2p:"))return from_bytes(Network::I2P,unhex(std::string_view(host).substr(4)),port);
    if(host.ends_with(".onion")) {
        if(host.size()!=62)throw std::invalid_argument("Tor v3 hostname length");
        auto raw=unbase32(std::string_view(host).substr(0,56),35);
        const auto check=onion_checksum(std::span<const std::uint8_t>(raw).first(32));
        if(raw[34]!=3 || raw[32]!=check[0] || raw[33]!=check[1])throw std::invalid_argument("Tor v3 checksum/version");
        return from_bytes(Network::TorV3,std::span<const std::uint8_t>(raw).first(32),port);
    }
    if(host.ends_with(".b32.i2p")) {
        if(host.size()!=60)throw std::invalid_argument("ordinary I2P base32 hostname length");
        return from_bytes(Network::I2P,unbase32(std::string_view(host).substr(0,52),32),port);
    }
    bool bracketed=host.front()=='[';
    if(bracketed) {if(host.back()!=']')throw std::invalid_argument("endpoint brackets");host=host.substr(1,host.size()-2);}
    // Windows' address parser also accepts abbreviated IPv4. Keep the contract
    // identical to Linux inet_pton, including IPv4 tails inside IPv6 literals.
    if(host.find(':')==host.npos) {
        if(bracketed)throw std::invalid_argument("brackets require IPv6");
        return from_bytes(Network::IPv4,ipv4_bytes(host),port);
    }
    if(host.find('.')!=host.npos)ipv4_bytes(std::string_view(host).substr(host.rfind(':')+1));
    boost::system::error_code ec;const auto ip=boost::asio::ip::make_address(host,ec);
    if(ec || (bracketed&&!ip.is_v6()))throw std::invalid_argument("endpoint must be a numeric address or supported overlay");
    if(ip.is_v4())return from_bytes(Network::IPv4,ip.to_v4().to_bytes(),port);
    return from_bytes(Network::IPv6,ip.to_v6().to_bytes(),port);
}
std::string Endpoint::host() const {
    if(network_==Network::IPv4){boost::asio::ip::address_v4::bytes_type b{};std::copy_n(address_.begin(),4,b.begin());return boost::asio::ip::address_v4(b).to_string();}
    if(network_==Network::IPv6){boost::asio::ip::address_v6::bytes_type b{};std::copy_n(address_.begin(),16,b.begin());return boost::asio::ip::address_v6(b).to_string();}
    return std::string(network_==Network::TorV3?"onion:":"i2p:")+hex(address());
}
std::string Endpoint::hostname() const {
    if(network_==Network::I2P)return base32(address())+".b32.i2p";
    if(network_==Network::TorV3){auto raw=std::vector<std::uint8_t>(address().begin(),address().end());const auto check=onion_checksum(address());raw.push_back(check[0]);raw.push_back(check[1]);raw.push_back(3);return base32(raw)+".onion";}
    return host();
}
std::string Endpoint::legacy_key() const {
    const auto prefix=network_==Network::TorV3?"torv3:":network_==Network::I2P?"i2p:":"";
    return prefix+host()+":"+std::to_string(port_.value());
}
std::vector<std::uint8_t> Endpoint::canonical_bytes() const {
    std::vector<std::uint8_t> out{1}; // Internal endpoint format, not a mesh message.
    out.insert(out.end(),bsty::MAINNET_MAGIC.begin(),bsty::MAINNET_MAGIC.end());
    out.push_back(static_cast<std::uint8_t>(network_));out.push_back(static_cast<std::uint8_t>(address().size()));
    out.insert(out.end(),address().begin(),address().end());out.push_back(static_cast<std::uint8_t>(port_.value()>>8));out.push_back(static_cast<std::uint8_t>(port_.value()));return out;
}
core::PeerId Endpoint::id() const {
    constexpr std::string_view domain="BSTY-FastSeed/PeerId/v1";
    std::vector<std::uint8_t> input(domain.begin(),domain.end());input.push_back(0);
    const auto bytes=canonical_bytes();input.insert(input.end(),bytes.begin(),bytes.end());return {digest(input,EVP_sha256())};
}
std::string id_hex(const core::PeerId& id) {return hex(id.bytes);}
std::optional<Endpoint> endpoint_from_key(std::string_view key) {
    if(key.size()>300)return std::nullopt;
    if(key.starts_with("torv3:"))key.remove_prefix(6);
    else if(key.starts_with("i2p:i2p:"))key.remove_prefix(4);
    const auto colon=key.rfind(':');if(colon==key.npos)return std::nullopt;
    std::uint64_t number=0;auto value=key.substr(colon+1);const auto result=std::from_chars(value.data(),value.data()+value.size(),number);
    const auto port=core::Port::from(number);
    if(result.ec!=std::errc{} || result.ptr!=value.data()+value.size() || !port)return std::nullopt;
    try{return Endpoint::parse(key.substr(0,colon),*port);}catch(const std::invalid_argument&){return std::nullopt;}
}
}
