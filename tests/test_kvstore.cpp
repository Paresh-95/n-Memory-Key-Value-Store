// Lightweight assert-based test suite (no external framework dependency).
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "KVStore.hpp"

using namespace kvstore;
using namespace std::chrono_literals;

namespace {

void testBasicCrud() {
    KVStore store;
    assert(!store.get("missing").has_value());

    store.set("a", "1");
    assert(store.get("a").value() == "1");
    assert(store.exists("a"));

    store.set("a", "2");
    assert(store.get("a").value() == "2");

    assert(store.del("a"));
    assert(!store.get("a").has_value());
    assert(!store.del("a"));

    std::cout << "[PASS] testBasicCrud" << std::endl;
}

void testTtlExpiration() {
    KVStore store;
    store.set("temp", "value", 50ms);
    assert(store.get("temp").value() == "value");
    assert(store.ttl("temp").has_value());

    std::this_thread::sleep_for(100ms);
    assert(!store.get("temp").has_value());
    assert(!store.exists("temp"));

    std::cout << "[PASS] testTtlExpiration" << std::endl;
}

void testExpireCommand() {
    KVStore store;
    store.set("k", "v");
    assert(store.ttl("k").value().count() == 0); // no expiry yet

    assert(store.expire("k", 30ms));
    auto remaining = store.ttl("k");
    assert(remaining.has_value() && remaining->count() > 0);

    std::this_thread::sleep_for(60ms);
    assert(!store.exists("k"));
    assert(!store.expire("missing", 10ms));

    std::cout << "[PASS] testExpireCommand" << std::endl;
}

void testLruEviction() {
    KVStore store(/*capacity=*/2);
    store.set("a", "1");
    store.set("b", "2");
    store.set("c", "3"); // evicts "a" (least recently used)

    assert(!store.exists("a"));
    assert(store.exists("b"));
    assert(store.exists("c"));
    assert(store.size() == 2);

    store.get("b");       // b is now most recently used
    store.set("d", "4");  // evicts "c"
    assert(store.exists("b"));
    assert(!store.exists("c"));
    assert(store.exists("d"));

    std::cout << "[PASS] testLruEviction" << std::endl;
}

void testConcurrentAccess() {
    KVStore store;
    constexpr int kThreads = 8;
    constexpr int kOpsPerThread = 2000;
    std::atomic<int> successfulReads{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < kOpsPerThread; ++i) {
                std::string key = "key" + std::to_string(t) + "_" + std::to_string(i % 50);
                store.set(key, std::to_string(i));
                if (store.get(key).has_value()) {
                    ++successfulReads;
                }
                if (i % 10 == 0) store.del(key);
            }
        });
    }
    for (auto& th : threads) th.join();

    assert(successfulReads.load() > 0);
    std::cout << "[PASS] testConcurrentAccess (successful reads: "
              << successfulReads.load() << ")" << std::endl;
}

} // namespace

int main() {
    testBasicCrud();
    testTtlExpiration();
    testExpireCommand();
    testLruEviction();
    testConcurrentAccess();

    std::cout << "All tests passed." << std::endl;
    return 0;
}
