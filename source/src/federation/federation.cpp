#include "fastseed/federation/federation.hpp"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace fastseed::federation {
namespace {
void put_u16(std::vector<std::uint8_t>& o, std::uint16_t v){o.push_back(v>>8);o.push_back(v);}
void put_u32(std::vector<std::uint8_t>& o, std::uint32_t v){for(int i=3;i>=0;--i)o.push_back(v>>(i*8));}
void put_u64(std::vector<std::uint8_t>& o, std::uint64_t v){for(int i=7;i>=0;--i)o.push_back(v>>(i*8));}
std::uint16_t u16(std::span<const std::uint8_t> b,size_t& p){if(p+2>b.size())throw std::runtime_error("truncated");auto v=(b[p]<<8)|b[p+1];p+=2;return v;}
std::uint32_t u32(std::span<const std::uint8_t> b,size_t& p){if(p+4>b.size())throw std::runtime_error("truncated");std::uint32_t v=0;for(int i=0;i<4;++i)v=(v<<8)|b[p++];return v;}
std::uint64_t u64(std::span<const std::uint8_t> b,size_t& p){if(p+8>b.size())throw std::runtime_error("truncated");std::uint64_t v=0;for(int i=0;i<8;++i)v=(v<<8)|b[p++];return v;}
std::string str(std::span<const std::uint8_t> b,size_t& p,size_t max){auto n=u16(b,p);if(n>max||p+n>b.size())throw std::runtime_error("invalid string");std::string s(reinterpret_cast<const char*>(b.data()+p),n);p+=n;return s;}
void put_str(std::vector<std::uint8_t>& o,std::string_view s){if(s.size()>65535)throw std::runtime_error("string too large");put_u16(o,(std::uint16_t)s.size());o.insert(o.end(),s.begin(),s.end());}
std::array<std::uint8_t,32> mac(std::span<const std::uint8_t> body,std::string_view secret){std::array<std::uint8_t,32> out{};unsigned int n=0;HMAC(EVP_sha256(),secret.data(),(int)secret.size(),body.data(),body.size(),out.data(),&n);if(n!=32)throw std::runtime_error("HMAC failure");return out;}
void put_i64(std::vector<std::uint8_t>& o,std::int64_t v){put_u64(o,(std::uint64_t)v);}
std::int64_t i64(std::span<const std::uint8_t> b,size_t&p){return (std::int64_t)u64(b,p);}
}
std::string PeerRecord::key() const{return host+":"+std::to_string(port);}
std::vector<std::uint8_t> encode(const FederationMessage& m,std::string_view secret,std::size_t max){
 if(m.sender_id.empty()||m.sender_id.size()>255||m.peers.size()>65535||m.withdrawals.size()>65535)throw std::runtime_error("invalid federation message");
 std::vector<std::uint8_t> o; o.reserve(128); o.insert(o.end(),{'B','F','S','1'});o.push_back((std::uint8_t)m.type);o.push_back(m.version);put_u16(o,0);put_u64(o,m.sequence);put_i64(o,m.timestamp_unix);put_str(o,m.sender_id);put_u16(o,(std::uint16_t)m.peers.size());
 for(auto const&p:m.peers){put_str(o,p.host);put_u16(o,p.port);put_u32(o,(std::uint32_t)p.protocol_version);put_u64(o,p.services);put_u64(o,p.successful_probes);put_u64(o,p.failed_probes);put_i64(o,p.last_success_unix);put_i64(o,p.last_failure_unix);put_u64(o,p.observed_at_unix);put_str(o,p.source_seeder);}
 put_u16(o,(std::uint16_t)m.withdrawals.size());for(auto const&k:m.withdrawals)put_str(o,k);
 auto tag=mac(o,secret);o.insert(o.end(),tag.begin(),tag.end());if(o.size()>max)throw std::runtime_error("federation message too large");return o;
}
FederationMessage decode(std::span<const std::uint8_t>w,std::string_view secret,const FederationConfig& c){
 if(w.size()<4+1+1+2+8+8+2+2+2+32||w.size()>c.max_message_size)throw std::runtime_error("invalid federation frame");
 if(!std::equal(w.begin(),w.begin()+4,std::array<std::uint8_t,4>{'B','F','S','1'}.begin()))throw std::runtime_error("bad federation magic");
 size_t p=4;FederationMessage m;m.type=(MessageType)w[p++];m.version=w[p++];(void)u16(w,p);m.sequence=u64(w,p);m.timestamp_unix=i64(w,p);m.sender_id=str(w,p,255);auto n=u16(w,p);if(n>c.max_peers_per_message)throw std::runtime_error("too many peers");
 for(unsigned i=0;i<n;++i){PeerRecord r;r.host=str(w,p,512);r.port=u16(w,p);r.protocol_version=(std::int32_t)u32(w,p);r.services=u64(w,p);r.successful_probes=u64(w,p);r.failed_probes=u64(w,p);r.last_success_unix=i64(w,p);r.last_failure_unix=i64(w,p);r.observed_at_unix=u64(w,p);r.source_seeder=str(w,p,255);m.peers.push_back(std::move(r));}
 auto wn=u16(w,p);if(wn>c.max_peers_per_message)throw std::runtime_error("too many withdrawals");for(unsigned i=0;i<wn;++i)m.withdrawals.push_back(str(w,p,1024));
 if(p+32!=w.size())throw std::runtime_error("trailing federation data");std::array<std::uint8_t,32> got{};std::copy(w.begin()+p,w.end(),got.begin());auto expected=mac(w.first(p),secret);if(!std::equal(got.begin(),got.end(),expected.begin()))throw std::runtime_error("authentication failed");m.auth_tag=got;return m;
}
bool ReplayGuard::accept(std::string_view sender,std::uint64_t seq,std::int64_t ts,std::int64_t now,std::chrono::seconds win){if(sender.empty()||seq==0)return false;auto it=entries_.find(std::string(sender));if(std::llabs(now-ts)>win.count())return false;if(it!=entries_.end()&&seq<=it->second.sequence)return false;if(entries_.size()>=max_entries_&&it==entries_.end())entries_.erase(entries_.begin());entries_[std::string(sender)]={seq,ts};return true;}
void ReplayGuard::clear(){entries_.clear();}
FederatedPeerStore::FederatedPeerStore(std::string local_seeder)
    : local_seeder_(std::move(local_seeder)) {}

std::size_t FederatedPeerStore::apply(const FederationMessage& message) {
    std::size_t changed = 0;
    for (const auto& peer : message.peers) {
        if (peer.host.empty() || peer.port == 0) continue;
        if (!peer.source_seeder.empty() && peer.source_seeder == local_seeder_) continue;
        const auto key = peer.key();
        auto it = peers_.find(key);
        bool replace = it == peers_.end();
        if (!replace) {
            const auto& old = it->second;
            replace = peer.observed_at_unix > old.observed_at_unix ||
                      (peer.observed_at_unix == old.observed_at_unix &&
                       peer.source_seeder > old.source_seeder);
        }
        if (replace) {
            peers_[key] = peer;
            ++changed;
        }
    }
    for (const auto& key : message.withdrawals) {
        auto it = peers_.find(key);
        if (it != peers_.end()) { peers_.erase(it); ++changed; }
    }
    return changed;
}

std::vector<PeerRecord> FederatedPeerStore::snapshot(std::size_t limit,
                                                      std::string_view exclude_source) const {
    std::vector<PeerRecord> out;
    out.reserve(std::min(limit, peers_.size()));
    for (const auto& [key, peer] : peers_) {
        (void)key;
        if (!exclude_source.empty() && peer.source_seeder == exclude_source) continue;
        out.push_back(peer);
    }
    std::sort(out.begin(), out.end(), [](const PeerRecord& a, const PeerRecord& b) {
        if (a.observed_at_unix != b.observed_at_unix) return a.observed_at_unix > b.observed_at_unix;
        return a.key() < b.key();
    });
    if (out.size() > limit) out.resize(limit);
    return out;
}

bool FederatedPeerStore::contains(std::string_view key) const {
    return peers_.find(std::string(key)) != peers_.end();
}

SeederMesh::SeederMesh(FederationConfig c):config_(std::move(c)),replay_(10000){if(config_.shared_secret.empty())throw std::invalid_argument("federation shared_secret must not be empty");}
std::vector<std::uint8_t> SeederMesh::make_hello(std::uint64_t seq,std::int64_t now)const{return encode(FederationMessage{MessageType::Hello,1,config_.seeder_id,seq,now,{},{}},config_.shared_secret,config_.max_message_size);}
std::vector<std::uint8_t> SeederMesh::make_sync(std::uint64_t seq,std::int64_t now,std::vector<PeerRecord> peers)const{if(peers.size()>config_.max_peers_per_message)throw std::runtime_error("too many peers");return encode(FederationMessage{MessageType::PeerSync,1,config_.seeder_id,seq,now,std::move(peers),{}},config_.shared_secret,config_.max_message_size);}
std::vector<std::uint8_t> SeederMesh::make_withdraw(std::uint64_t seq,std::int64_t now,std::vector<std::string> keys)const{if(keys.size()>config_.max_peers_per_message)throw std::runtime_error("too many withdrawals");return encode(FederationMessage{MessageType::PeerWithdraw,1,config_.seeder_id,seq,now,{},std::move(keys)},config_.shared_secret,config_.max_message_size);}
bool SeederMesh::accept(std::span<const std::uint8_t>w,std::int64_t now,FederationMessage&out){try{auto m=decode(w,config_.shared_secret,config_);if(m.sender_id==config_.seeder_id)return false;if(!replay_.accept(m.sender_id,m.sequence,m.timestamp_unix,now,config_.replay_window))return false;out=std::move(m);return true;}catch(...){return false;}}
}
