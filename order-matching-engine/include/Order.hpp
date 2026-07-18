#pragma once

#include <chrono>
#include <cstdint>

namespace matching {

enum class Side { Buy, Sell };

struct Order {
    std::int64_t id = 0;
    Side side = Side::Buy;
    double price = 0.0;
    std::int64_t quantity = 0; // remaining, unfilled quantity
    std::chrono::steady_clock::time_point timestamp;
};

struct Trade {
    std::int64_t buyOrderId = 0;
    std::int64_t sellOrderId = 0;
    double price = 0.0;
    std::int64_t quantity = 0;
};

} // namespace matching
