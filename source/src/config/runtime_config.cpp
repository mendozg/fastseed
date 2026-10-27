#include "fastseed/config/runtime_config.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace fastseed::config {
namespace {

struct Document {
    std::unordered_map<std::string, std::string> values;
    std::vector<std::string> errors;
};

std::string trim(std::string s) {
    auto not_space=[](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

std::string strip_comment(const std::string& line) {
    bool quoted=false, escaped=false;
    for (std::size_t i=0;i<line.size();++i) {
        char c=line[i];
        if (escaped) { escaped=false; continue; }
        if (quoted && c=='\\') { escaped=true; continue; }
        if (c=='"') { quoted=!quoted; continue; }
        if (!quoted && c=='#') return line.substr(0,i);
    }
    return line;
}

bool valid_key(std::string_view k) {
    if(k.empty()) return false;
    for(char c:k) if(!(std::isalnum(static_cast<unsigned char>(c))||c=='_'||c=='-'||c=='.')) return false;
    return true;
}

bool parse_document(const std::string& path, Document& d) {
    std::ifstream in(path);
    if(!in) { d.errors.push_back("cannot open configuration: "+path); return false; }
    std::string section, line; std::size_t lineno=0;
    while(std::getline(in,line)) {
        ++lineno; line=trim(strip_comment(line)); if(line.empty()) continue;
        if(line.front()=='[') {
            if(line.size()<3 || line.back()!=']' || line[1]=='[') { d.errors.push_back("line "+std::to_string(lineno)+": invalid table header"); continue; }
            section=trim(line.substr(1,line.size()-2));
            if(!valid_key(section)) d.errors.push_back("line "+std::to_string(lineno)+": invalid table name");
            continue;
        }
        auto eq=line.find('=');
        if(eq==std::string::npos) { d.errors.push_back("line "+std::to_string(lineno)+": expected key = value"); continue; }
        auto key=trim(line.substr(0,eq)); auto value=trim(line.substr(eq+1));
        if(!valid_key(key) || value.empty()) { d.errors.push_back("line "+std::to_string(lineno)+": invalid key/value"); continue; }
        // Support TOML-style multiline arrays used by the seed list.
        if(value.front()=='[' && value.find(']')==std::string::npos) {
            std::string continuation;
            bool closed=false;
            while(std::getline(in,continuation)) {
                ++lineno;
                continuation=trim(strip_comment(continuation));
                if(!continuation.empty()) value += continuation;
                if(value.find(']')!=std::string::npos) { closed=true; break; }
            }
            if(!closed) { d.errors.push_back("line "+std::to_string(lineno)+": unterminated array"); continue; }
        }
        std::string full=section.empty()?key:section+"."+key;
        if(d.values.contains(full)) d.errors.push_back("line "+std::to_string(lineno)+": duplicate key "+full);
        d.values[full]=value;
    }
    return d.errors.empty();
}

std::string unquote(const std::string& raw, bool& ok) {
    ok=false; auto s=trim(raw);
    if(s.size()<2 || s.front()!='"' || s.back()!='"') return {};
    std::string out; out.reserve(s.size()-2);
    bool esc=false;
    for(std::size_t i=1;i+1<s.size();++i) {
        char c=s[i];
        if(esc) { if(c=='"'||c=='\\'||c=='n'||c=='t'||c=='r') { out.push_back(c=='n'?'\n':c=='t'?'\t':c=='r'?'\r':c); esc=false; } else return {}; }
        else if(c=='\\') esc=true; else out.push_back(c);
    }
    if(esc) return {};
    ok=true; return out;
}

bool get_string(const Document& d,const char* key,std::string& out,std::vector<std::string>& e) {
    auto it=d.values.find(key); if(it==d.values.end()) return true;
    bool ok=false; out=unquote(it->second,ok); if(!ok) e.push_back(std::string(key)+" must be a quoted string"); return ok;
}

template<class T> bool parse_unsigned(const std::string& raw,T& out) {
    auto s=trim(raw); if(s.empty()||s[0]=='-') return false; std::size_t p=0;
    try { unsigned long long v=std::stoull(s,&p,0); if(p!=s.size()||v>static_cast<unsigned long long>(std::numeric_limits<T>::max())) return false; out=static_cast<T>(v); return true; } catch(...) { return false; }
}
template<class T> bool parse_signed(const std::string& raw,T& out) {
    auto s=trim(raw); std::size_t p=0;
    try { long long v=std::stoll(s,&p,0); if(p!=s.size()||v<std::numeric_limits<T>::min()||v>std::numeric_limits<T>::max()) return false; out=static_cast<T>(v); return true; } catch(...) { return false; }
}
bool parse_double(const std::string& raw,double& out) { try { std::size_t p=0; out=std::stod(trim(raw),&p); return p==trim(raw).size(); } catch(...) { return false; } }
bool parse_bool(const std::string& raw,bool& out) { auto s=trim(raw); if(s=="true"){out=true;return true;} if(s=="false"){out=false;return true;} return false; }

template<class T, class F> void assign_u(const Document& d,const char* key,T& field,std::vector<std::string>& e,F&& check) {
    auto it=d.values.find(key); if(it==d.values.end()) return; T v{}; if(!parse_unsigned(it->second,v)||!check(v)) e.push_back(std::string(key)+" has an invalid value"); else field=v;
}
template<class T> void assign_u(const Document& d,const char* key,T& field,std::vector<std::string>& e) { assign_u(d,key,field,e,[](T v){return v>0;}); }

template<class T> void assign_s(const Document& d,const char* key,T& field,std::vector<std::string>& e) { auto it=d.values.find(key); if(it==d.values.end()) return; T v{}; if(!parse_signed(it->second,v)) e.push_back(std::string(key)+" has an invalid value"); else field=v; }
void assign_bool(const Document& d,const char* key,bool& field,std::vector<std::string>& e) { auto it=d.values.find(key); if(it==d.values.end()) return; bool v{}; if(!parse_bool(it->second,v)) e.push_back(std::string(key)+" must be true or false"); else field=v; }
void assign_double(const Document& d,const char* key,double& field,std::vector<std::string>& e,double lo,double hi) { auto it=d.values.find(key); if(it==d.values.end()) return; double v{}; if(!parse_double(it->second,v)||v<lo||v>hi) e.push_back(std::string(key)+" has an invalid value"); else field=v; }

bool parse_string_array(const std::string& raw,std::vector<std::string>& out) {
    auto s=trim(raw); if(s.size()<2||s.front()!='['||s.back()!=']') return false;
    std::size_t i=1; std::vector<std::string> vals;
    while(i<s.size()-1) {
        while(i<s.size()-1 && (std::isspace(static_cast<unsigned char>(s[i]))||s[i]==',')) ++i;
        if(i>=s.size()-1) break;
        if(s[i]!='"') return false;
        std::size_t j=i+1; bool esc=false;
        for(;j<s.size()-1;++j){ if(esc){esc=false;continue;} if(s[j]=='\\'){esc=true;continue;} if(s[j]=='"')break; }
        if(j>=s.size()-1||s[j]!='"') return false;
        bool ok=false; auto v=unquote(s.substr(i,j-i+1),ok); if(!ok) return false; vals.push_back(std::move(v)); i=j+1;
        while(i<s.size()-1&&std::isspace(static_cast<unsigned char>(s[i])))++i;
        if(i<s.size()-1&&s[i]!=',') return false;
    }
    out=std::move(vals); return true;
}
void assign_array(const Document& d,const char* key,std::vector<std::string>& field,std::vector<std::string>& e) { auto it=d.values.find(key); if(it==d.values.end()) return; std::vector<std::string> v; if(!parse_string_array(it->second,v)||v.empty()) e.push_back(std::string(key)+" must be a non-empty string array"); else field=std::move(v); }

void unknown_keys(const Document& d,std::vector<std::string>& e) {
    static const std::unordered_set<std::string> known={
      "network.network","network.p2p_port","network.protocol_version","network.min_peer_version",
      "bootstrap.dns_seeds","crawler.max_concurrent_probes","crawler.max_candidates","crawler.max_open_connections","crawler.connect_timeout_ms","crawler.handshake_timeout_ms","crawler.addr_exchange_timeout_ms","crawler.enable_tor_transport","crawler.enable_i2p_transport",
      "crawler.cooldown.initial_seconds","crawler.cooldown.maximum_seconds","crawler.cooldown.backoff_multiplier",
      "cache.hot_ipv4","cache.hot_ipv6","cache.warm_max","cache.cold_max",
      "health.minimum_observations_for_hot","health.freshness_seconds","health.quarantine_seconds","health.cooldown_initial_seconds","health.cooldown_max_seconds","health.cooldown_multiplier","health.quarantine_after_failures","health.latency_history_size",
      "database.path","database.max_peers","database.stale_after_days","database.failed_after_days","database.busy_timeout_ms",
      "database.persistence.max_items","database.persistence.batch_size","database.persistence.flush_interval_ms",
      "dns.nameserver","dns.soa_mailbox","dns.soa_serial","dns.zone","dns.listen_address","dns.listen_port","dns.ttl_seconds","dns.max_answers","dns.include_warm","dns.max_response_size","dns.refresh_interval_seconds",
      "dns.rate_limit.per_source_per_second","dns.rate_limit.global_per_second","dns.rate_limit.max_sources",
      "network_intelligence.enabled","network_intelligence.learning_half_life_seconds","network_intelligence.minimum_observations","network_intelligence.reliability_weight","network_intelligence.latency_weight","network_intelligence.freshness_weight","network_intelligence.diversity_weight",
      "federation.seeder_id","federation.shared_secret","federation.clock_skew_seconds","federation.replay_window_seconds","federation.max_peers_per_message","federation.max_message_size",
      "scheduler.max_batch","scheduler.stale_refresh_seconds","scheduler.cold_refresh_seconds","scheduler.warm_refresh_seconds","scheduler.hot_refresh_seconds","scheduler.minimum_refresh_seconds","scheduler.maximum_refresh_seconds","scheduler.exploration_fraction","scheduler.minimum_exploration","scheduler.maximum_exploration","scheduler.low_confidence_threshold","scheduler.exploration_rotation_seconds",
      "scheduler.direct_tcp.max_inflight","scheduler.direct_tcp.max_batch","scheduler.tor_socks5.max_inflight","scheduler.tor_socks5.max_batch","scheduler.i2p_sam.max_inflight","scheduler.i2p_sam.max_batch",
      "scheduler.high_availability","scheduler.low_availability","scheduler.high_latency_ms","scheduler.low_latency_ms",
      "transport.tor.proxy_host","transport.tor.proxy_port","transport.i2p.sam_host","transport.i2p.sam_port","transport.i2p.session_id","transport.i2p.local_nick"
    };
    for(const auto& [k,v]:d.values) if(!known.contains(k)) e.push_back("unknown configuration key: "+k);
}

} // namespace

RuntimeConfig RuntimeConfig::defaults() { return {}; }

ValidationResult validate(const RuntimeConfig& c) {
    ValidationResult r;
    auto bad=[&](const std::string& s){r.errors.push_back(s);};
    if(c.network.network!="mainnet") bad("network.network must be mainnet");
    if(c.network.p2p_port!=bsty::DEFAULT_P2P_PORT) bad("network.p2p_port must be 8226 for BSTY mainnet");
    if(c.network.protocol_version<bsty::MIN_PEER_VERSION) bad("network.protocol_version is below the minimum peer protocol");
    if(c.network.min_peer_version<bsty::MIN_PEER_VERSION) bad("network.min_peer_version is below the BSTY minimum");
    if(c.dns.engine.zone.empty()) bad("dns.zone must not be empty");
    try { dns::DnsResponseEngine check_names(c.dns.engine); }
    catch(const std::invalid_argument& e) { bad(e.what()); }
    if(c.dns.server.port==0) bad("dns.listen_port must be non-zero");
    if(c.dns.engine.max_response_size<512||c.dns.engine.max_response_size>65535) bad("dns.max_response_size must be between 512 and 65535");
    if(c.dns.engine.max_answers==0) bad("dns.max_answers must be non-zero");
    if(c.crawler.max_concurrent_probes==0) bad("crawler.max_concurrent_probes must be non-zero");
    if(c.crawler.max_open_connections==0) bad("crawler.max_open_connections must be non-zero");
    if(c.crawler.max_candidates==0) bad("crawler.max_candidates must be non-zero");
    if(c.crawler.connect_timeout.count()<=0||c.crawler.handshake_timeout.count()<=0) bad("crawler timeouts must be positive");
    if(c.crawler.cooldown_initial.count()<=0||c.crawler.cooldown_max<c.crawler.cooldown_initial) bad("crawler cooldown range is invalid");
    if(c.crawler.cooldown_multiplier<1.0) bad("crawler.cooldown.backoff_multiplier must be >= 1");
    if(c.database.path.empty()) bad("database.path must not be empty");
    if(c.database.max_peers==0) bad("database.max_peers must be non-zero");
    const double weights=c.intelligence.reliability_weight+c.intelligence.latency_weight+c.intelligence.freshness_weight+c.intelligence.diversity_weight;
    if(c.network_intelligence_enabled && (weights<=0.0||weights>1.000001)) bad("network_intelligence weights must sum to >0 and <=1");
    if(c.scheduler.minimum_refresh.count()<=0||c.scheduler.maximum_refresh<c.scheduler.minimum_refresh) bad("scheduler refresh range is invalid");
    if(c.scheduler.exploration_fraction<0.0||c.scheduler.exploration_fraction>1.0) bad("scheduler.exploration_fraction must be between 0 and 1");
    if(c.scheduler.low_confidence_threshold<0.0||c.scheduler.low_confidence_threshold>1.0) bad("scheduler.low_confidence_threshold must be between 0 and 1");
    if(c.scheduler.minimum_exploration > c.scheduler.maximum_exploration) bad("scheduler exploration bounds are invalid");
    if(c.scheduler.high_availability < c.scheduler.low_availability) bad("scheduler availability thresholds are invalid");
    if(c.scheduler.high_latency.count() <= 0 || c.scheduler.low_latency < c.scheduler.high_latency) bad("scheduler latency thresholds are invalid");
    if(c.scheduler.direct_tcp.max_inflight==0 || c.scheduler.tor_socks5.max_inflight==0 || c.scheduler.i2p_sam.max_inflight==0) bad("scheduler transport quotas must be non-zero");
    if(c.database.busy_timeout_ms < 0) bad("database.busy_timeout_ms must be non-negative");
    if(c.persistence.max_items==0 || c.persistence.batch_size==0 || c.persistence.flush_interval.count()<=0) bad("database persistence settings must be non-zero");
    if(c.dns.engine.rate_limit.per_source_per_second==0||c.dns.engine.rate_limit.global_per_second==0||c.dns.engine.rate_limit.max_sources==0) bad("dns rate limits must be non-zero");
    r.ok=r.errors.empty(); return r;
}

std::optional<RuntimeConfig> load_file(const std::string& path,std::vector<std::string>& errors) {
    Document d; parse_document(path,d); errors=d.errors; if(!errors.empty()) return std::nullopt;
    unknown_keys(d,errors);
    RuntimeConfig c=RuntimeConfig::defaults();
    auto S=[&](const char* k,std::string& f){get_string(d,k,f,errors);};
    S("network.network",c.network.network); assign_u(d,"network.p2p_port",c.network.p2p_port,errors); assign_s(d,"network.protocol_version",c.network.protocol_version,errors); assign_s(d,"network.min_peer_version",c.network.min_peer_version,errors);
    assign_array(d,"bootstrap.dns_seeds",c.bootstrap.dns_seeds,errors);
    assign_u(d,"crawler.max_concurrent_probes",c.crawler.max_concurrent_probes,errors); assign_u(d,"crawler.max_candidates",c.crawler.max_candidates,errors); assign_u(d,"crawler.max_open_connections",c.crawler.max_open_connections,errors);
    std::uint64_t u{}; assign_u(d,"crawler.connect_timeout_ms",u,errors); if(d.values.contains("crawler.connect_timeout_ms")) c.crawler.connect_timeout=std::chrono::milliseconds(u); assign_u(d,"crawler.handshake_timeout_ms",u,errors); if(d.values.contains("crawler.handshake_timeout_ms")) c.crawler.handshake_timeout=std::chrono::milliseconds(u); assign_u(d,"crawler.addr_exchange_timeout_ms",u,errors); if(d.values.contains("crawler.addr_exchange_timeout_ms")) c.crawler.addr_exchange_timeout=std::chrono::milliseconds(u);
    assign_bool(d,"crawler.enable_tor_transport",c.crawler.enable_tor_transport,errors); assign_bool(d,"crawler.enable_i2p_transport",c.crawler.enable_i2p_transport,errors);
    assign_u(d,"crawler.cooldown.initial_seconds",u,errors); if(d.values.contains("crawler.cooldown.initial_seconds")) c.crawler.cooldown_initial=std::chrono::seconds(u); assign_u(d,"crawler.cooldown.maximum_seconds",u,errors); if(d.values.contains("crawler.cooldown.maximum_seconds")) c.crawler.cooldown_max=std::chrono::seconds(u); assign_double(d,"crawler.cooldown.backoff_multiplier",c.crawler.cooldown_multiplier,errors,1.0,100.0);
    assign_u(d,"cache.hot_ipv4",c.health.hot_ipv4,errors); assign_u(d,"cache.hot_ipv6",c.health.hot_ipv6,errors); assign_u(d,"cache.warm_max",c.health.warm_max,errors); assign_u(d,"cache.cold_max",c.health.cold_max,errors);
    assign_u(d,"health.minimum_observations_for_hot",c.health.minimum_observations_for_hot,errors); assign_u(d,"health.freshness_seconds",u,errors); if(d.values.contains("health.freshness_seconds")) c.health.freshness=std::chrono::seconds(u); assign_u(d,"health.quarantine_seconds",u,errors); if(d.values.contains("health.quarantine_seconds")) c.health.quarantine=std::chrono::seconds(u); assign_u(d,"health.cooldown_initial_seconds",u,errors); if(d.values.contains("health.cooldown_initial_seconds")) c.health.cooldown_initial=std::chrono::seconds(u); assign_u(d,"health.cooldown_max_seconds",u,errors); if(d.values.contains("health.cooldown_max_seconds")) c.health.cooldown_max=std::chrono::seconds(u); assign_double(d,"health.cooldown_multiplier",c.health.cooldown_multiplier,errors,1.0,100.0); assign_u(d,"health.quarantine_after_failures",c.health.quarantine_after_failures,errors); assign_u(d,"health.latency_history_size",c.health.latency_history_size,errors);
    S("database.path",c.database.path); assign_u(d,"database.max_peers",c.database.max_peers,errors); assign_u(d,"database.stale_after_days",u,errors); if(d.values.contains("database.stale_after_days")) c.database.stale_after=std::chrono::hours(24*u); assign_u(d,"database.failed_after_days",u,errors); if(d.values.contains("database.failed_after_days")) c.database.failed_after=std::chrono::hours(24*u); std::int64_t si{}; assign_s(d,"database.busy_timeout_ms",si,errors); if(d.values.contains("database.busy_timeout_ms")) c.database.busy_timeout_ms=static_cast<int>(si);
    assign_u(d,"database.persistence.max_items",c.persistence.max_items,errors); assign_u(d,"database.persistence.batch_size",c.persistence.batch_size,errors); assign_u(d,"database.persistence.flush_interval_ms",u,errors); if(d.values.contains("database.persistence.flush_interval_ms")) c.persistence.flush_interval=std::chrono::milliseconds(u);
    S("dns.zone",c.dns.engine.zone);
    S("dns.nameserver",c.dns.engine.nameserver); S("dns.soa_mailbox",c.dns.engine.soa_mailbox);
    assign_u(d,"dns.soa_serial",c.dns.engine.soa_serial,errors);
    if (auto it=d.values.find("dns.listen_address"); it!=d.values.end()) {
        std::string address; bool ok=false; address=unquote(it->second,ok);
        if(!ok) errors.push_back("dns.listen_address must be a quoted IP address");
        else { boost::system::error_code ec; auto parsed=boost::asio::ip::make_address(address,ec); if(ec) errors.push_back("dns.listen_address must be a valid IP address"); else c.dns.server.bind_address=parsed; }
    }
    assign_u(d,"dns.listen_port",c.dns.server.port,errors); c.dns.engine.listen_port=c.dns.server.port; assign_u(d,"dns.ttl_seconds",c.dns.engine.ttl_seconds,errors); assign_u(d,"dns.max_answers",c.dns.engine.max_answers,errors); assign_bool(d,"dns.include_warm",c.dns.engine.include_warm,errors); assign_u(d,"dns.max_response_size",c.dns.engine.max_response_size,errors); assign_u(d,"dns.refresh_interval_seconds",u,errors);
    assign_u(d,"dns.rate_limit.per_source_per_second",c.dns.engine.rate_limit.per_source_per_second,errors); assign_u(d,"dns.rate_limit.global_per_second",c.dns.engine.rate_limit.global_per_second,errors); assign_u(d,"dns.rate_limit.max_sources",c.dns.engine.rate_limit.max_sources,errors); c.dns.server.rate_limit=c.dns.engine.rate_limit;
    assign_bool(d,"network_intelligence.enabled",c.network_intelligence_enabled,errors); assign_u(d,"network_intelligence.learning_half_life_seconds",u,errors); if(d.values.contains("network_intelligence.learning_half_life_seconds")) c.intelligence.learning_half_life=std::chrono::seconds(u); assign_u(d,"network_intelligence.minimum_observations",c.intelligence.minimum_observations,errors); assign_double(d,"network_intelligence.reliability_weight",c.intelligence.reliability_weight,errors,0,1); assign_double(d,"network_intelligence.latency_weight",c.intelligence.latency_weight,errors,0,1); assign_double(d,"network_intelligence.freshness_weight",c.intelligence.freshness_weight,errors,0,1); assign_double(d,"network_intelligence.diversity_weight",c.intelligence.diversity_weight,errors,0,1);
    S("federation.seeder_id",c.federation.seeder_id); S("federation.shared_secret",c.federation.shared_secret); assign_u(d,"federation.clock_skew_seconds",u,errors); if(d.values.contains("federation.clock_skew_seconds")) c.federation.clock_skew=std::chrono::seconds(u); assign_u(d,"federation.replay_window_seconds",u,errors); if(d.values.contains("federation.replay_window_seconds")) c.federation.replay_window=std::chrono::seconds(u); assign_u(d,"federation.max_peers_per_message",c.federation.max_peers_per_message,errors); assign_u(d,"federation.max_message_size",c.federation.max_message_size,errors);
    assign_u(d,"scheduler.max_batch",c.scheduler.max_batch,errors); assign_u(d,"scheduler.stale_refresh_seconds",u,errors); if(d.values.contains("scheduler.stale_refresh_seconds")) c.scheduler.stale_refresh=std::chrono::seconds(u); assign_u(d,"scheduler.cold_refresh_seconds",u,errors); if(d.values.contains("scheduler.cold_refresh_seconds")) c.scheduler.cold_refresh=std::chrono::seconds(u); assign_u(d,"scheduler.warm_refresh_seconds",u,errors); if(d.values.contains("scheduler.warm_refresh_seconds")) c.scheduler.warm_refresh=std::chrono::seconds(u); assign_u(d,"scheduler.hot_refresh_seconds",u,errors); if(d.values.contains("scheduler.hot_refresh_seconds")) c.scheduler.hot_refresh=std::chrono::seconds(u); assign_u(d,"scheduler.minimum_refresh_seconds",u,errors); if(d.values.contains("scheduler.minimum_refresh_seconds")) c.scheduler.minimum_refresh=std::chrono::seconds(u); assign_u(d,"scheduler.maximum_refresh_seconds",u,errors); if(d.values.contains("scheduler.maximum_refresh_seconds")) c.scheduler.maximum_refresh=std::chrono::seconds(u); assign_double(d,"scheduler.exploration_fraction",c.scheduler.exploration_fraction,errors,0,1); assign_u(d,"scheduler.minimum_exploration",c.scheduler.minimum_exploration,errors); assign_u(d,"scheduler.maximum_exploration",c.scheduler.maximum_exploration,errors); assign_double(d,"scheduler.low_confidence_threshold",c.scheduler.low_confidence_threshold,errors,0,1); assign_u(d,"scheduler.exploration_rotation_seconds",u,errors); if(d.values.contains("scheduler.exploration_rotation_seconds")) c.scheduler.exploration_rotation=std::chrono::seconds(u);
    assign_u(d,"scheduler.direct_tcp.max_inflight",c.scheduler.direct_tcp.max_inflight,errors); assign_u(d,"scheduler.direct_tcp.max_batch",c.scheduler.direct_tcp.max_batch,errors); assign_u(d,"scheduler.tor_socks5.max_inflight",c.scheduler.tor_socks5.max_inflight,errors); assign_u(d,"scheduler.tor_socks5.max_batch",c.scheduler.tor_socks5.max_batch,errors); assign_u(d,"scheduler.i2p_sam.max_inflight",c.scheduler.i2p_sam.max_inflight,errors); assign_u(d,"scheduler.i2p_sam.max_batch",c.scheduler.i2p_sam.max_batch,errors); assign_double(d,"scheduler.high_availability",c.scheduler.high_availability,errors,0,1); assign_double(d,"scheduler.low_availability",c.scheduler.low_availability,errors,0,1); assign_u(d,"scheduler.high_latency_ms",u,errors); if(d.values.contains("scheduler.high_latency_ms")) c.scheduler.high_latency=std::chrono::milliseconds(u); assign_u(d,"scheduler.low_latency_ms",u,errors); if(d.values.contains("scheduler.low_latency_ms")) c.scheduler.low_latency=std::chrono::milliseconds(u);
    S("transport.tor.proxy_host",c.crawler.tor_transport.proxy_host); assign_u(d,"transport.tor.proxy_port",c.crawler.tor_transport.proxy_port,errors); S("transport.i2p.sam_host",c.crawler.i2p_transport.sam_host); assign_u(d,"transport.i2p.sam_port",c.crawler.i2p_transport.sam_port,errors); S("transport.i2p.session_id",c.crawler.i2p_transport.session_id); S("transport.i2p.local_nick",c.crawler.i2p_transport.local_nick);
    // Materialize scheduler settings into the crawler's adaptive controller.
    c.crawler.scheduler = c.scheduler;
    c.crawler.feedback.global_max_inflight = c.crawler.max_concurrent_probes;
    c.crawler.feedback.direct_tcp = {1, c.scheduler.direct_tcp.max_inflight, c.scheduler.direct_tcp.max_inflight};
    c.crawler.feedback.tor_socks5 = {2, c.scheduler.tor_socks5.max_inflight, c.scheduler.tor_socks5.max_inflight};
    c.crawler.feedback.i2p_sam = {2, c.scheduler.i2p_sam.max_inflight, c.scheduler.i2p_sam.max_inflight};
    c.crawler.feedback.increase_success_rate = c.scheduler.high_availability;
    c.crawler.feedback.decrease_success_rate = c.scheduler.low_availability;
    c.crawler.feedback.increase_latency_ms = static_cast<double>(c.scheduler.high_latency.count());
    c.crawler.feedback.decrease_latency_ms = static_cast<double>(c.scheduler.low_latency.count());
    c.crawler.feedback.learning_half_life = std::chrono::duration_cast<std::chrono::hours>(c.intelligence.learning_half_life);
    c.crawler.feedback.cooldown_after_adjustment = std::chrono::seconds(30);
    auto vr=validate(c); errors.insert(errors.end(),vr.errors.begin(),vr.errors.end());
    if(!errors.empty()) return std::nullopt;
    return c;
}

ValidationResult validate_file(const std::string& path) { std::vector<std::string> errors; auto c=load_file(path,errors); ValidationResult r; r.ok=static_cast<bool>(c); r.errors=std::move(errors); return r; }
std::string version_string(){return "BSTY-FastSeed 1.0.0 (Production)";}
}
