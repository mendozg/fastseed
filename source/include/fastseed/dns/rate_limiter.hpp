#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fastseed::dns {

class RateLimiter {
public:
    struct Config {
        std::size_t per_source_per_second{100};
        std::size_t global_per_second{5000};
        std::size_t max_sources{4096};
    };

    RateLimiter();
    explicit RateLimiter(Config config);

    bool allow(std::string_view source,
               std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void reset();
    std::size_t tracked_sources() const noexcept;

private:
    struct Bucket { std::chrono::steady_clock::time_point window{}; std::size_t count{}; };
    Config config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Bucket> sources_;
    Bucket global_{};
};

} // namespace fastseed::dns
