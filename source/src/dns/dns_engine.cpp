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

bool read_name(std::span<const std::uint8_t> q, std::size_t& pos, std::string& name) {
    std::string out;
    std::size_t labels = 0;
    while (pos < q.size()) {
        const auto len = q[pos++];
        if (len == 0) { name = out.empty() ? "." : out.substr(0, out.size()-1); return true; }
        if ((len & 0xc0) != 0 || len > 63 || pos + len > q.size() || ++labels > 127) return false;
        out.append(reinterpret_cast<const char*>(q.data() + pos), len);
        out.push_back('.');
        pos += len;
    }
    return false;
}

bool is_ipv4(std::string_view h) { boost::system::error_code ec; auto a=boost::asio::ip::make_address(std::string(h),ec); return !ec && a.is_v4(); }
bool is_ipv6(std::string_view h) { boost::system::error_code ec; auto a=boost::asio::ip::make_address(std::string(h),ec); return !ec && a.is_v6(); }
}

void SnapshotStore::publish(std::shared_ptr<const DnsSnapshot> snapshot) noexcept { snapshot_.store(std::move(snapshot), std::memory_order_release); }
std::shared_ptr<const DnsSnapshot> SnapshotStore::load() const noexcept { return snapshot_.load(std::memory_order_acquire); }

DnsResponseEngine::DnsResponseEngine(DnsConfig config) : config_(std::move(config)) {
    config_.zone = normalize_name(config_.zone);
    if (config_.ttl_seconds == 0) config_.ttl_seconds = 60;
    if (config_.max_answers == 0) config_.max_answers = 1;
    if (config_.max_response_size < 512) config_.max_response_size = 512;
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
    if (query.size() < 12) return QueryResult::Malformed;
    const auto flags = read16(query.data()+2);
    if ((flags & 0x8000) != 0 || (flags & 0x7800) != 0) return QueryResult::Refused;
    if ((flags & 0x0100) == 0) { /* RD is irrelevant to authoritative seed answers */ }
    if (read16(query.data()+4) != 1 || read16(query.data()+6) != 0 || read16(query.data()+8) != 0 || read16(query.data()+10) != 0) return QueryResult::Refused;
    std::size_t pos=12;
    if (!read_name(query,pos,question.name) || pos+4 > query.size()) return QueryResult::Malformed;
    question.name=normalize_name(question.name); question.type=read16(query.data()+pos); question.klass=read16(query.data()+pos+2); pos+=4;
    if (question.klass != CLASS_IN || !name_matches_zone(question.name)) return QueryResult::Refused;
    if (question.type != TYPE_A && question.type != TYPE_AAAA) return QueryResult::NoData;
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

std::vector<std::uint8_t> DnsResponseEngine::build_response(std::span<const std::uint8_t> query) const {
    std::vector<std::uint8_t> out;
    if (query.size() < 12) return out;
    const auto id=read16(query.data()); DnsQuestion q; const auto status=parse_question(query,q);
    std::uint16_t flags=0x8400; // QR + AA
    if ((read16(query.data()+2)&0x0100)!=0) flags|=0x0100;
    if (status==QueryResult::Malformed) flags=0x8000 | 0x0001; // QR + FORMERR
    else if (status==QueryResult::Refused) flags=0x8000 | 0x0005; // QR + REFUSED
    std::vector<DnsAnswer> answers;
    if (status==QueryResult::Answered) { auto s=snapshot(); if(s) answers=select_answers(q,*s); }
    put16(out,id); put16(out,flags); put16(out,1); put16(out,static_cast<std::uint16_t>(answers.size())); put16(out,0); put16(out,0);
    // Echo the original question exactly; bounded to 4096 bytes by the server.
    std::size_t qend=12;
    if (status!=QueryResult::Malformed) { std::string n; if(read_name(query,qend,n) && qend+4<=query.size()) qend+=4; else qend=query.size(); }
    else qend=query.size();
    out.insert(out.end(),query.begin()+12,query.begin()+static_cast<std::ptrdiff_t>(qend));
    for(const auto& a:answers){ put16(out,0xc00c); put16(out,a.type); put16(out,CLASS_IN); put32(out,a.ttl); put16(out,static_cast<std::uint16_t>(a.address.size())); out.insert(out.end(),a.address.begin(),a.address.end()); }
    if (out.size() > config_.max_response_size) {
        // Rebuild a bounded response with the TC bit and no answers rather than fragmenting UDP.
        out.resize(12);
        const auto current_flags = read16(out.data()+2);
        out[2] = static_cast<std::uint8_t>((current_flags | 0x02) >> 8);
        out[3] = static_cast<std::uint8_t>(current_flags | 0x02);
        out[6] = out[7] = 0;
        std::size_t qend = 12; std::string n;
        if (read_name(query, qend, n) && qend + 4 <= query.size()) qend += 4;
        if (qend <= query.size()) out.insert(out.end(), query.begin()+12, query.begin()+static_cast<std::ptrdiff_t>(qend));
    }
    return out;
}

UdpServer::UdpServer(boost::asio::io_context& io, DnsResponseEngine& engine, std::uint16_t port, const boost::asio::ip::address& bind_address, RateLimiter::Config rate_limit)
    : socket_(io), engine_(engine), limiter_(rate_limit) { boost::system::error_code ec; socket_.open(bind_address.is_v6()?boost::asio::ip::udp::v6():boost::asio::ip::udp::v4(),ec); if(ec) throw boost::system::system_error(ec); socket_.set_option(boost::asio::socket_base::reuse_address(true),ec); socket_.bind({bind_address,port},ec); if(ec) throw boost::system::system_error(ec); }
void UdpServer::start(){ receive(); }
void UdpServer::stop(){ boost::system::error_code ec; socket_.cancel(ec); socket_.close(ec); }
void UdpServer::receive(){ socket_.async_receive_from(boost::asio::buffer(buffer_),remote_,[this](boost::system::error_code ec,std::size_t n){ if(!ec && n>0){ const auto source = remote_.address().to_string(); if(limiter_.allow(source)){ response_=engine_.build_response(std::span<const std::uint8_t>(buffer_.data(),n)); send(response_.size()); } else if(socket_.is_open()) receive(); } else if(socket_.is_open()) receive(); }); }
void UdpServer::send(std::size_t length){ socket_.async_send_to(boost::asio::buffer(response_.data(),length),remote_,[this](boost::system::error_code, std::size_t){ if(socket_.is_open()) receive(); }); }

} // namespace fastseed::dns
