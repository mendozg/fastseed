#include "fastseed/dns/rate_limiter.hpp"
#include "fastseed/protocol/protocol_limits.hpp"
#include <algorithm>

namespace fastseed::dns {

RateLimiter::RateLimiter() : RateLimiter(Config{}) {}

RateLimiter::RateLimiter(Config config) : config_(config) {
    if (config_.per_source_per_second == 0) config_.per_source_per_second = 1;
    if (config_.global_per_second == 0) config_.global_per_second = 1;
    if (config_.max_sources == 0) config_.max_sources = 1;
}

bool RateLimiter::allow(std::string_view source, std::chrono::steady_clock::time_point now) {
    if (source.size() > protocol::limits::max_dns_source_size) return false;
    std::lock_guard lock(mutex_);
    const auto one_second = std::chrono::seconds(1);
    if (global_.window.time_since_epoch().count() == 0 || now - global_.window >= one_second) {
        global_ = Bucket{now, 0};
    }

    // Evaluate the per-source bucket before consuming the global token. This
    // prevents a single abusive source from exhausting global capacity with
    // requests that are already over its own quota.
    Bucket* source_bucket = nullptr;
    if (!source.empty()) {
        auto it = sources_.find(std::string(source));
        if (it == sources_.end()) {
            if (sources_.size() >= config_.max_sources) {
                auto victim = sources_.begin();
                for (auto cur = sources_.begin(); cur != sources_.end(); ++cur) {
                    if (cur->second.window < victim->second.window ||
                        (cur->second.window == victim->second.window && cur->second.count < victim->second.count))
                        victim = cur;
                }
                sources_.erase(victim);
            }
            it = sources_.emplace(std::string(source), Bucket{now, 0}).first;
        } else if (now - it->second.window >= one_second) {
            it->second = Bucket{now, 0};
        }
        if (it->second.count >= config_.per_source_per_second) return false;
        source_bucket = &it->second;
    }

    if (global_.count >= config_.global_per_second) return false;
    ++global_.count;
    if (source_bucket) ++source_bucket->count;
    return true;
}

void RateLimiter::reset() {
    std::lock_guard lock(mutex_);
    sources_.clear();
    global_ = {};
}

std::size_t RateLimiter::tracked_sources() const noexcept {
    std::lock_guard lock(mutex_);
    return sources_.size();
}

} // namespace fastseed::dns
