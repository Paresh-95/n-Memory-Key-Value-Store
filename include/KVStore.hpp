#pragma once

#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace kvstore {

// Thread-safe in-memory key-value store with TTL expiration and optional
// LRU eviction. Reads take a shared lock; writes (and LRU bookkeeping,
// which touches state even on "read" access) take an exclusive lock.
class KVStore {
public:
    using Clock = std::chrono::steady_clock;

    // capacity == 0 means unbounded (no LRU eviction).
    // reapIntervalMs controls how often the background thread sweeps for
    // expired keys; it does not affect correctness since GET/EXISTS/TTL
    // also lazily check expiration.
    explicit KVStore(std::size_t capacity = 0,
                      std::chrono::milliseconds reapInterval = std::chrono::milliseconds(500));
    ~KVStore();

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    // Inserts or overwrites a key. ttl of zero means no expiration.
    void set(const std::string& key, std::string value,
              std::chrono::milliseconds ttl = std::chrono::milliseconds(0));

    std::optional<std::string> get(const std::string& key);

    bool del(const std::string& key);

    bool exists(const std::string& key);

    // Remaining time-to-live. nullopt if the key doesn't exist (or already
    // expired); zero-value duration means the key exists with no expiry.
    std::optional<std::chrono::milliseconds> ttl(const std::string& key);

    // Refresh/replace TTL on an existing key. Returns false if key is absent.
    bool expire(const std::string& key, std::chrono::milliseconds ttl);

    std::size_t size();

    void clear();

private:
    struct Entry {
        std::string value;
        std::optional<Clock::time_point> expiresAt;
        std::list<std::string>::iterator lruIt;
    };

    bool isExpiredLocked(const Entry& entry) const;
    void touchLru(const std::string& key, Entry& entry);
    void evictIfNeededLocked();
    void reapLoop();

    std::size_t capacity_;
    std::chrono::milliseconds reapInterval_;

    std::shared_mutex mutex_;
    std::unordered_map<std::string, Entry> store_;
    std::list<std::string> lruOrder_; // front = most recently used

    std::thread reaperThread_;
    std::mutex reaperMutex_;
    std::condition_variable_any reaperCv_;
    bool stopping_ = false;
};

} // namespace kvstore
