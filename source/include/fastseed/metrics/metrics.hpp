#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace fastseed::metrics {

class Counter {
public:
    void inc(std::uint64_t n = 1) noexcept { value_.fetch_add(n, std::memory_order_relaxed); }
    std::uint64_t value() const noexcept { return value_.load(std::memory_order_relaxed); }
private:
    std::atomic<std::uint64_t> value_{0};
};

class Gauge {
public:
    void set(std::int64_t n) noexcept { value_.store(n, std::memory_order_relaxed); }
    void inc(std::int64_t n = 1) noexcept { value_.fetch_add(n, std::memory_order_relaxed); }
    void dec(std::int64_t n = 1) noexcept { value_.fetch_sub(n, std::memory_order_relaxed); }
    std::int64_t value() const noexcept { return value_.load(std::memory_order_relaxed); }
private:
    std::atomic<std::int64_t> value_{0};
};

class Registry {
public:
    Counter& counter(std::string_view name);
    Gauge& gauge(std::string_view name);
    std::string prometheus() const;
    void clear();
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Counter>> counters_;
    std::unordered_map<std::string, std::unique_ptr<Gauge>> gauges_;
};

} // namespace fastseed::metrics
