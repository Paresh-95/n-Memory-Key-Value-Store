// Simple throughput benchmark for KVStore under concurrent load.
// Usage: kvstore_bench [threads] [ops_per_thread]
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

#include "KVStore.hpp"

int main(int argc, char** argv) {
    int numThreads = argc >= 2 ? std::atoi(argv[1]) : std::thread::hardware_concurrency();
    int opsPerThread = argc >= 3 ? std::atoi(argv[2]) : 100000;
    if (numThreads <= 0) numThreads = 4;

    kvstore::KVStore store;
    std::atomic<long long> totalOps{0};

    auto worker = [&](int id) {
        for (int i = 0; i < opsPerThread; ++i) {
            std::string key = "key" + std::to_string(id) + "_" + std::to_string(i % 1000);
            store.set(key, "value");
            store.get(key);
            totalOps.fetch_add(2, std::memory_order_relaxed);
        }
    };

    auto start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (int t = 0; t < numThreads; ++t) threads.emplace_back(worker, t);
    for (auto& th : threads) th.join();

    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    double opsPerSec = static_cast<double>(totalOps.load()) / seconds;

    std::cout << "Threads:        " << numThreads << "\n"
              << "Ops per thread: " << opsPerThread << " (SET+GET pairs)\n"
              << "Total ops:      " << totalOps.load() << "\n"
              << "Elapsed:        " << seconds << "s\n"
              << "Throughput:     " << static_cast<long long>(opsPerSec) << " ops/sec\n";

    return 0;
}
