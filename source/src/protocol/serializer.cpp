#include "fastseed/protocol/serializer.hpp"
#include <stdexcept>
#include <cstring>
#include <algorithm>
namespace fastseed::protocol {
static void put_le(std::vector<std::uint8_t>& d, std::uint64_t v, int n){ for(int i=0;i<n;++i)d.push_back(static_cast<std::uint8_t>(v>>(8*i))); }
void Writer::u8(std::uint8_t v){data_.push_back(v);} void Writer::i32(std::int32_t v){put_le(data_,static_cast<std::uint32_t>(v),4);} void Writer::u32(std::uint32_t v){put_le(data_,v,4);} void Writer::u64(std::uint64_t v){put_le(data_,v,8);}
void Writer::bytes(std::span<const std::uint8_t> v){data_.insert(data_.end(),v.begin(),v.end());}
void Writer::string(const std::string& s){ if(s.size()>0xFFFF) throw std::length_error("string too long"); if(s.size()<253){u8(static_cast<std::uint8_t>(s.size()));} else {u8(253); data_.push_back(static_cast<std::uint8_t>(s.size())); data_.push_back(static_cast<std::uint8_t>(s.size()>>8));} bytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()),s.size())); }
void Writer::fixed16(std::span<const std::uint8_t> v){ if(v.size()!=16) throw std::invalid_argument("fixed16"); bytes(v); }
void Reader::need(std::size_t n) const { if(n>remaining()) throw std::runtime_error("truncated input"); }
std::uint8_t Reader::u8(){need(1);return data_[pos_++];} std::int32_t Reader::i32(){return static_cast<std::int32_t>(u32());} std::uint32_t Reader::u32(){need(4);std::uint32_t v=0;for(int i=0;i<4;++i)v|=static_cast<std::uint32_t>(data_[pos_++])<<(8*i);return v;} std::uint64_t Reader::u64(){need(8);std::uint64_t v=0;for(int i=0;i<8;++i)v|=static_cast<std::uint64_t>(data_[pos_++])<<(8*i);return v;}
std::vector<std::uint8_t> Reader::bytes(std::size_t n){need(n);auto b=data_.subspan(pos_,n);pos_+=n;return {b.begin(),b.end()};}
std::string Reader::string(std::size_t n){auto b=bytes(n);return std::string(reinterpret_cast<const char*>(b.data()),b.size());}
std::array<std::uint8_t,16> Reader::fixed16(){std::array<std::uint8_t,16>a{};need(16);std::copy_n(data_.begin()+pos_,16,a.begin());pos_+=16;return a;}
}
