#include "fastseed/dns/dns_engine.hpp"
#include <boost/asio/ip/address.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <random>
#include <stdexcept>

namespace fastseed::dns {
namespace {
std::uint16_t read16(const std::uint8_t* p) { return (std::uint16_t(p[0]) << 8) | p[1]; }
void put16(std::vector<std::uint8_t>& o, std::uint16_t v) { o.push_back(v >> 8); o.push_back(v & 0xff); }
void put32(std::vector<std::uint8_t>& o, std::uint32_t v) { o.push_back(v >> 24); o.push_back(v >> 16); o.push_back(v >> 8); o.push_back(v); }
std::string lower(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }

// Decode compressed names with backward-only pointers and a bounded expanded length.
bool read_name(std::span<const std::uint8_t> q, std::size_t& pos, std::string& name) {
    std::size_t at=pos, end=0, expanded=1;
    std::string out;
    for (std::size_t steps=0; steps<256 && at<q.size(); ++steps) {
        const auto len=q[at++];
        if (!len) { pos=end?end:at; name=out.empty()?".":out.substr(0,out.size()-1); return true; }
        if ((len&0xc0)==0xc0) {
            if(at>=q.size()) return false;
            const std::size_t target=((len&0x3f)<<8)|q[at++];
            if(target<12 || target>=at-2) return false;
            if(!end) end=at;
            at=target; continue;
        }
        if((len&0xc0) || at+len>q.size() || (expanded+=len+1)>255) return false;
        // Host names are compared as label strings; embedded dots would alias labels.
        for(std::size_t i=0;i<len;++i) if(q[at+i]=='.' || q[at+i]==0) return false;
        out.append(reinterpret_cast<const char*>(q.data()+at),len); out.push_back('.'); at+=len;
    }
    return false;
}
void put_name(std::vector<std::uint8_t>& out, std::string_view name) {
    if(name==".") { out.push_back(0); return; }
    std::size_t start=0;
    while(start<name.size()) {
        auto end=name.find('.',start); if(end==std::string_view::npos) end=name.size();
        out.push_back(static_cast<std::uint8_t>(end-start));
        out.insert(out.end(),name.begin()+start,name.begin()+end); start=end+1;
    }
    out.push_back(0);
}
bool valid_config_name(std::string_view s) {
    if(s.empty() || s.size()>253) return false;
    std::size_t start=0;
    while(start<s.size()) { auto end=s.find('.',start); if(end==std::string_view::npos) end=s.size();
        if(end==start || end-start>63) return false;
        for(auto i=start;i<end;++i) if(!std::isalnum(static_cast<unsigned char>(s[i])) && s[i]!='-' && s[i]!='_') return false;
        start=end+1;
    }
    return true;
}

bool is_ipv4(std::string_view h) { boost::system::error_code ec; auto a=boost::asio::ip::make_address(std::string(h),ec); return !ec && a.is_v4(); }
bool is_ipv6(std::string_view h) { boost::system::error_code ec; auto a=boost::asio::ip::make_address(std::string(h),ec); return !ec && a.is_v6(); }
}

void SnapshotStore::publish(std::shared_ptr<const DnsSnapshot> snapshot) noexcept { std::atomic_store(&snapshot_, std::move(snapshot)); }
std::shared_ptr<const DnsSnapshot> SnapshotStore::load() const noexcept { return std::atomic_load(&snapshot_); }

DnsResponseEngine::DnsResponseEngine(DnsConfig config) : config_(std::move(config)) {
    config_.zone = normalize_name(config_.zone);
    if (config_.ttl_seconds == 0) config_.ttl_seconds = 60;
    if (config_.max_answers == 0) config_.max_answers = 1;
    config_.max_response_size=std::clamp<std::size_t>(config_.max_response_size,512,65535);
    config_.nameserver=normalize_name(config_.nameserver.empty()?config_.zone:config_.nameserver);
    config_.soa_mailbox=normalize_name(config_.soa_mailbox.empty()?"hostmaster."+config_.zone:config_.soa_mailbox);
    if(!valid_config_name(config_.zone) || !valid_config_name(config_.nameserver) || !valid_config_name(config_.soa_mailbox))
        throw std::invalid_argument("Invalid DNS zone, nameserver or SOA mailbox");
}

void DnsResponseEngine::publish_from_cache(const health::PeerHealthCache& cache, std::uint64_t generation) {
    auto s = std::make_shared<DnsSnapshot>();
    s->peers = cache.dns_snapshot(config_.max_answers * 4, config_.include_warm);
    s->generation = generation;
    store_.publish(std::move(s));
}
std::shared_ptr<const DnsSnapshot> DnsResponseEngine::snapshot() const noexcept { return store_.load(); }

std::string DnsResponseEngine::normalize_name(std::string_view name) {
    std::string s(name);
    while (s.size() > 1 && s.back() == '.') s.pop_back();
    return lower(std::move(s));
}
bool DnsResponseEngine::name_matches_zone(std::string_view name) const { return normalize_name(name) == config_.zone; }

QueryResult DnsResponseEngine::parse_question(std::span<const std::uint8_t> query, DnsQuestion& question) const {
    question=DnsQuestion{};
    if(query.size()<12) return QueryResult::Malformed;
    const auto flags=read16(query.data()+2);
    if(read16(query.data()+4)!=1) return QueryResult::Malformed;
    std::size_t pos=12;
    if(!read_name(query,pos,question.name) || pos+4>query.size()) return QueryResult::Malformed;
    question.type=read16(query.data()+pos); question.klass=read16(query.data()+pos+2); pos+=4;
    question.name=normalize_name(question.name); question.valid=true;
    if(read16(query.data()+6) || read16(query.data()+8)) return QueryResult::Malformed;
    const auto additional=read16(query.data()+10);
    for(std::size_t i=0;i<additional;++i) {
        std::string owner; const auto owner_start=pos;
        if(!read_name(query,pos,owner) || pos+10>query.size()) return QueryResult::Malformed;
        auto type=read16(query.data()+pos), klass=read16(query.data()+pos+2);
        auto len=read16(query.data()+pos+8); const auto ttl=pos+4; pos+=10;
        if(pos+len>query.size()) return QueryResult::Malformed;
        if(type==41) {
            if(question.edns || owner!="." || query[owner_start]!=0) return QueryResult::Malformed;
            question.edns=true; question.udp_size=std::max<std::uint16_t>(512,klass);
            question.edns_version=query[ttl+1]; question.dnssec_ok=(query[ttl+2]&0x80)!=0;
            auto option=pos;
            while(option<pos+len) {
                if(option+4>pos+len) return QueryResult::Malformed;
                const auto size=read16(query.data()+option+2); option+=4;
                if(option+size>pos+len) return QueryResult::Malformed;
                option+=size;
            }
        }
        pos+=len;
    }
    if(pos!=query.size() || (flags&0x0040)) return QueryResult::Malformed;
    if(flags&0x7800) return QueryResult::NotImplemented;
    if(question.edns_version) return QueryResult::BadVersion;
    if(question.klass!=CLASS_IN) return QueryResult::Refused;
    if(!name_matches_zone(question.name)) {
        const auto suffix="."+config_.zone;
        if(question.name.size()>suffix.size() && question.name.compare(question.name.size()-suffix.size(),suffix.size(),suffix)==0)
            return QueryResult::NameError;
        return QueryResult::Refused;
    }
    if(question.type!=TYPE_A && question.type!=TYPE_AAAA && question.type!=2 && question.type!=6) return QueryResult::NoData;
    return QueryResult::Answered;
}

std::vector<DnsAnswer> DnsResponseEngine::select_answers(const DnsQuestion& q, const DnsSnapshot& snapshot) const {
    std::vector<DnsAnswer> out;
    for (const auto& p : snapshot.peers) {
        const bool match = (q.type == TYPE_A && is_ipv4(p.host)) || (q.type == TYPE_AAAA && is_ipv6(p.host));
        if (!match) continue;
        boost::system::error_code ec; auto a=boost::asio::ip::make_address(p.host,ec); if(ec) continue;
        DnsAnswer aout; aout.host=p.host; aout.type=q.type; aout.ttl=config_.ttl_seconds;
        if (a.is_v4()) { auto bytes=a.to_v4().to_bytes(); aout.address.assign(bytes.begin(),bytes.end()); } else { auto bytes=a.to_v6().to_bytes(); aout.address.assign(bytes.begin(),bytes.end()); } out.push_back(std::move(aout));
        if (out.size() >= config_.max_answers) break;
    }
    return out;
}

std::vector<std::uint8_t> DnsResponseEngine::build_response(std::span<const std::uint8_t> query, bool tcp) const {
    // QR packets are responses, never queries; silently discard to avoid loops.
    if(query.size()<12 || (read16(query.data()+2)&0x8000)) return {};
    DnsQuestion q; const auto status=parse_question(query,q);
    const bool authoritative=status==QueryResult::Answered || status==QueryResult::NoData || status==QueryResult::NameError;
    std::uint16_t flags=0x8000 | (read16(query.data()+2)&0x7900);
    if(authoritative) flags|=0x0400;
    if(status==QueryResult::Malformed) flags|=1;
    if(status==QueryResult::Refused) flags|=5;
    if(status==QueryResult::NotImplemented) flags|=4;
    if(status==QueryResult::NameError) flags|=3;
    const bool opt=q.edns && status!=QueryResult::Malformed;
    const std::size_t limit=tcp?65535:std::min<std::size_t>(config_.max_response_size,q.edns?q.udp_size:512);
    std::vector<std::uint8_t> out;
    put16(out,read16(query.data())); put16(out,flags); put16(out,q.valid?1:0);
    put16(out,0); put16(out,0); put16(out,0);
    if(q.valid) {
        // Preserve question case for resolvers using DNS 0x20 case randomization.
        std::size_t pos=12; std::string original_name; read_name(query,pos,original_name);
        put_name(out,original_name); put16(out,q.type); put16(out,q.klass);
    }
    std::uint16_t answers=0, authority=0;
    bool truncated=false;
    auto append_rr=[&](std::uint16_t type,const std::vector<std::uint8_t>& data,bool auth) {
        std::vector<std::uint8_t> owner;
        if(auth) put_name(owner,config_.zone); else put16(owner,0xc00c);
        if(out.size()+owner.size()+10+data.size()+(opt?11:0)>limit) { truncated=true; return; }
        out.insert(out.end(),owner.begin(),owner.end()); put16(out,type); put16(out,CLASS_IN); put32(out,config_.ttl_seconds);
        put16(out,static_cast<std::uint16_t>(data.size())); out.insert(out.end(),data.begin(),data.end());
        if(auth) ++authority; else ++answers;
    };
    auto soa=[&](bool auth) {
        std::vector<std::uint8_t> data; put_name(data,config_.nameserver); put_name(data,config_.soa_mailbox);
        for(auto v:{config_.soa_serial,3600u,600u,86400u,config_.ttl_seconds}) put32(data,v);
        append_rr(6,data,auth);
    };
    if(status==QueryResult::Answered) {
        if(q.type==6) soa(false);
        else if(q.type==2) { std::vector<std::uint8_t> data; put_name(data,config_.nameserver); append_rr(2,data,false); }
        else {
            auto s=snapshot(); auto selected=s?select_answers(q,*s):std::vector<DnsAnswer>{};
            // Never send a partial RRset: truncate the complete answer set for UDP.
            std::size_t bytes=0; for(const auto& a:selected) bytes+=12+a.address.size();
            if(out.size()+bytes+(opt?11:0)>limit) truncated=true;
            else for(const auto& a:selected) append_rr(a.type,a.address,false);
            if(selected.empty()) soa(true);
        }
    } else if(status==QueryResult::NoData || status==QueryResult::NameError) soa(true);
    if(truncated) flags|=0x0200;
    out[2]=flags>>8; out[3]=flags&255;
    out[6]=answers>>8; out[7]=answers&255; out[8]=authority>>8; out[9]=authority&255;
    if(opt) {
        out[11]=1; out.push_back(0); put16(out,41); put16(out,static_cast<std::uint16_t>(config_.max_response_size));
        // BADVERS=16 is carried in OPT's extended RCODE; EDNS version/flags are zero.
        put32(out,(status==QueryResult::BadVersion?0x01000000u:0u) | (q.dnssec_ok?0x8000u:0u)); put16(out,0);
    }
    return out;
}

UdpServer::UdpServer(boost::asio::io_context& io, DnsResponseEngine& engine, std::uint16_t port, const boost::asio::ip::address& bind_address, RateLimiter::Config rate_limit)
    : socket_(io), engine_(engine), limiter_(rate_limit) { boost::system::error_code ec; socket_.open(bind_address.is_v6()?boost::asio::ip::udp::v6():boost::asio::ip::udp::v4(),ec); if(ec) throw boost::system::system_error(ec); socket_.set_option(boost::asio::socket_base::reuse_address(true),ec); socket_.bind({bind_address,port},ec); if(ec) throw boost::system::system_error(ec); }
void UdpServer::start(){ receive(); }
void UdpServer::stop(){ boost::system::error_code ec; socket_.cancel(ec); socket_.close(ec); }
void UdpServer::receive(){ socket_.async_receive_from(boost::asio::buffer(buffer_),remote_,[this](boost::system::error_code ec,std::size_t n){ if(!ec && n>0){ const auto source = remote_.address().to_string(); if(limiter_.allow(source)){ response_=engine_.build_response(std::span<const std::uint8_t>(buffer_.data(),n)); if(response_.empty()) { if(socket_.is_open()) receive(); } else send(response_.size()); } else if(socket_.is_open()) receive(); } else if(socket_.is_open()) receive(); }); }
void UdpServer::send(std::size_t length){ socket_.async_send_to(boost::asio::buffer(response_.data(),length),remote_,[this](boost::system::error_code, std::size_t){ if(socket_.is_open()) receive(); }); }

} // namespace fastseed::dns
