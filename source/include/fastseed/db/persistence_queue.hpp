#pragma once

#include "fastseed/db/peer_database.hpp"
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace fastseed::db {

class PersistenceQueue {
public:
    struct Config {
        std::size_t max_items{10000};
        std::size_t batch_size{128};
        std::chrono::milliseconds flush_interval{500};
    };

    explicit PersistenceQueue(PeerDatabase& database);
    PersistenceQueue(PeerDatabase& database, Config config);
    ~PersistenceQueue();
    PersistenceQueue(const PersistenceQueue&) = delete;
    PersistenceQueue& operator=(const PersistenceQueue&) = delete;

    bool enqueue(health::PeerHealth peer);
    void flush();
    void stop();
    std::size_t pending() const;
    std::uint64_t committed() const noexcept;
    std::uint64_t dropped() const noexcept;

private:
    void worker();

    PeerDatabase& database_;
    Config config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable flushed_cv_;
    std::queue<health::PeerHealth> queue_;
    std::thread worker_;
    bool stopping_{false};
    bool flush_requested_{false};
    std::uint64_t committed_{0};
    std::uint64_t dropped_{0};
};

} // namespace fastseed::db
