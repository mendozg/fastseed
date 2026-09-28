#include "fastseed/db/persistence_queue.hpp"
#include <algorithm>

namespace fastseed::db {

PersistenceQueue::PersistenceQueue(PeerDatabase& database)
    : PersistenceQueue(database, Config{}) {}

PersistenceQueue::PersistenceQueue(PeerDatabase& database, Config config)
    : database_(database), config_(config), worker_(&PersistenceQueue::worker, this) {
    if (config_.max_items == 0) config_.max_items = 1;
    if (config_.batch_size == 0) config_.batch_size = 1;
    if (config_.flush_interval.count() < 1) config_.flush_interval = std::chrono::milliseconds(1);
}

PersistenceQueue::~PersistenceQueue() { stop(); }

bool PersistenceQueue::enqueue(health::PeerHealth peer) {
    {
        std::lock_guard lock(mutex_);
        if (stopping_ || queue_.size() >= config_.max_items) { ++dropped_; return false; }
        queue_.push(std::move(peer));
    }
    cv_.notify_one();
    return true;
}

void PersistenceQueue::flush() {
    std::unique_lock lock(mutex_);
    flush_requested_ = true;
    cv_.notify_one();
    flushed_cv_.wait(lock, [this] { return queue_.empty() && !flush_requested_; });
}

void PersistenceQueue::stop() {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
        flush_requested_ = true;
    }
    cv_.notify_one();
    if (worker_.joinable()) worker_.join();
}

std::size_t PersistenceQueue::pending() const { std::lock_guard lock(mutex_); return queue_.size(); }
std::uint64_t PersistenceQueue::committed() const noexcept { std::lock_guard lock(mutex_); return committed_; }
std::uint64_t PersistenceQueue::dropped() const noexcept { std::lock_guard lock(mutex_); return dropped_; }

void PersistenceQueue::worker() {
    std::unique_lock lock(mutex_);
    while (true) {
        cv_.wait_for(lock, config_.flush_interval, [this] { return stopping_ || flush_requested_ || queue_.size() >= config_.batch_size; });
        const bool should_stop = stopping_ && queue_.empty();
        if (should_stop) break;
        std::vector<health::PeerHealth> batch;
        const auto count = std::min(config_.batch_size, queue_.size());
        batch.reserve(count);
        while (!queue_.empty() && batch.size() < config_.batch_size) { batch.push_back(std::move(queue_.front())); queue_.pop(); }
        if (queue_.empty()) flush_requested_ = false;
        lock.unlock();
        try {
            if (!batch.empty()) { database_.save_batch(batch); std::lock_guard g(mutex_); committed_ += batch.size(); }
        } catch (...) {
            std::lock_guard g(mutex_);
            while (!batch.empty() && queue_.size() < config_.max_items) { queue_.push(std::move(batch.back())); batch.pop_back(); }
            if (batch.size()) dropped_ += batch.size();
            flush_requested_ = false;
        }
        lock.lock();
        flushed_cv_.notify_all();
    }
    flushed_cv_.notify_all();
}

} // namespace fastseed::db
