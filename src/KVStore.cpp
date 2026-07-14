#include "KVStore.hpp"

namespace kvstore {

KVStore::KVStore(std::size_t capacity, std::chrono::milliseconds reapInterval)
    : capacity_(capacity), reapInterval_(reapInterval) {
    reaperThread_ = std::thread(&KVStore::reapLoop, this);
}

KVStore::~KVStore() {
    {
        std::lock_guard<std::mutex> lock(reaperMutex_);
        stopping_ = true;
    }
    reaperCv_.notify_all();
    if (reaperThread_.joinable()) {
        reaperThread_.join();
    }
}

bool KVStore::isExpiredLocked(const Entry& entry) const {
    return entry.expiresAt.has_value() && Clock::now() >= *entry.expiresAt;
}

void KVStore::touchLru(const std::string& key, Entry& entry) {
    lruOrder_.erase(entry.lruIt);
    lruOrder_.push_front(key);
    entry.lruIt = lruOrder_.begin();
}

void KVStore::evictIfNeededLocked() {
    if (capacity_ == 0) return;
    while (store_.size() > capacity_) {
        const std::string& lruKey = lruOrder_.back();
        store_.erase(lruKey);
        lruOrder_.pop_back();
    }
}

void KVStore::set(const std::string& key, std::string value, std::chrono::milliseconds ttl) {
    std::unique_lock lock(mutex_);
    std::optional<Clock::time_point> expiresAt;
    if (ttl.count() > 0) {
        expiresAt = Clock::now() + ttl;
    }

    auto it = store_.find(key);
    if (it != store_.end()) {
        it->second.value = std::move(value);
        it->second.expiresAt = expiresAt;
        touchLru(key, it->second);
        return;
    }

    lruOrder_.push_front(key);
    Entry entry{std::move(value), expiresAt, lruOrder_.begin()};
    store_.emplace(key, std::move(entry));
    evictIfNeededLocked();
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::unique_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return std::nullopt;
    if (isExpiredLocked(it->second)) {
        lruOrder_.erase(it->second.lruIt);
        store_.erase(it);
        return std::nullopt;
    }
    touchLru(key, it->second);
    return it->second.value;
}

bool KVStore::del(const std::string& key) {
    std::unique_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    lruOrder_.erase(it->second.lruIt);
    store_.erase(it);
    return true;
}

bool KVStore::exists(const std::string& key) {
    std::shared_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    return !isExpiredLocked(it->second);
}

std::optional<std::chrono::milliseconds> KVStore::ttl(const std::string& key) {
    std::shared_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || isExpiredLocked(it->second)) return std::nullopt;
    if (!it->second.expiresAt.has_value()) return std::chrono::milliseconds(0);
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        *it->second.expiresAt - Clock::now());
}

bool KVStore::expire(const std::string& key, std::chrono::milliseconds ttl) {
    std::unique_lock lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end() || isExpiredLocked(it->second)) return false;
    it->second.expiresAt = ttl.count() > 0
        ? std::optional<Clock::time_point>(Clock::now() + ttl)
        : std::nullopt;
    return true;
}

std::size_t KVStore::size() {
    std::shared_lock lock(mutex_);
    return store_.size();
}

void KVStore::clear() {
    std::unique_lock lock(mutex_);
    store_.clear();
    lruOrder_.clear();
}

void KVStore::reapLoop() {
    std::unique_lock<std::mutex> reaperLock(reaperMutex_);
    while (!stopping_) {
        reaperCv_.wait_for(reaperLock, reapInterval_, [this] { return stopping_; });
        if (stopping_) break;

        std::unique_lock lock(mutex_);
        auto now = Clock::now();
        for (auto it = store_.begin(); it != store_.end();) {
            if (it->second.expiresAt.has_value() && now >= *it->second.expiresAt) {
                lruOrder_.erase(it->second.lruIt);
                it = store_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

} // namespace kvstore
