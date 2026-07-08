// Throughput benchmark for OrderBook matching.
// Usage: matching_bench [num_orders]
//
// Feeds a mix of resting and crossing orders through a single OrderBook on
// one thread (matching a real matching engine's single-writer design) and
// reports orders/sec.
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>

#include "OrderBook.hpp"

int main(int argc, char** argv) {
    int numOrders = argc >= 2 ? std::atoi(argv[1]) : 500000;

    matching::OrderBook book;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> sideDist(0, 1);
    std::uniform_real_distribution<double> priceDist(95.0, 105.0);
    std::uniform_int_distribution<std::int64_t> qtyDist(1, 20);

    auto start = std::chrono::steady_clock::now();

    std::int64_t totalTrades = 0;
    for (int i = 0; i < numOrders; ++i) {
        matching::Order order;
        order.id = i + 1;
        order.side = sideDist(rng) == 0 ? matching::Side::Buy : matching::Side::Sell;
        order.price = std::round(priceDist(rng) * 100.0) / 100.0;
        order.quantity = qtyDist(rng);
        order.timestamp = std::chrono::steady_clock::now();

        totalTrades += static_cast<std::int64_t>(book.addOrder(order).size());
    }

    auto end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();

    std::cout << "Orders processed: " << numOrders << "\n"
              << "Trades generated: " << totalTrades << "\n"
              << "Elapsed:          " << seconds << "s\n"
              << "Throughput:       " << static_cast<long long>(numOrders / seconds) << " orders/sec\n";

    return 0;
}
