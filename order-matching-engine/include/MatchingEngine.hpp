#pragma once

#include <atomic>
#include <vector>

#include "Order.hpp"
#include "OrderBook.hpp"

namespace matching {

// Ties order-id assignment, input validation, and trade logging to a
// single OrderBook. Intended to be driven by exactly one thread (see
// OrderQueue) so the underlying book never needs its own locking.
class MatchingEngine {
public:
    // Submits a new order for the given side/price/quantity. Assigns and
    // returns the new order's id, and appends any resulting trades to
    // `outTrades`. Throws OrderError for invalid input (caller decides
    // whether to log and continue, or propagate).
    std::int64_t submit(Side side, double price, std::int64_t quantity,
                          std::vector<Trade>& outTrades);

    bool cancel(std::int64_t orderId);

    const OrderBook& book() const { return book_; }

private:
    OrderBook book_;
    std::atomic<std::int64_t> nextOrderId_{1};
};

} // namespace matching
