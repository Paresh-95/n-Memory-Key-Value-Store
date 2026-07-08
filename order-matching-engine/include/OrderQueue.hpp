#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace matching {

// Thread-safe single-consumer queue: any number of producer threads may
// push commands (new orders, cancels, ...); exactly one consumer thread
// (the matching engine's worker) should call pop() in a loop. This is the
// concurrency boundary of the whole system -- the order book itself is
// single-threaded by design, and this queue is what lets multiple
// submitters feed it safely without ever letting two threads touch the
// book at once.
template <typename T>
class OrderQueue {
public:
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    // Blocks until an item is available or the queue is stopped.
    // Returns nullopt once stopped and drained -- the consumer's signal to exit.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (queue_.empty()) return std::nullopt;

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool stopping_ = false;
};

} // namespace matching
