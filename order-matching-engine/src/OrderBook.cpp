#include "OrderBook.hpp"

#include <algorithm>

namespace matching {

std::vector<Trade> OrderBook::addOrder(Order order) {
    if (order.price <= 0.0) throw OrderError("price must be positive");
    if (order.quantity <= 0) throw OrderError("quantity must be positive");

    std::vector<Trade> trades;

    if (order.side == Side::Buy) {
        while (order.quantity > 0 && !asks_.empty() && asks_.begin()->first <= order.price) {
            auto levelIt = asks_.begin();
            std::deque<Order>& level = levelIt->second;

            while (order.quantity > 0 && !level.empty()) {
                Order& resting = level.front();
                std::int64_t filled = std::min(order.quantity, resting.quantity);

                trades.push_back(Trade{order.id, resting.id, resting.price, filled});
                order.quantity -= filled;
                resting.quantity -= filled;

                if (resting.quantity == 0) {
                    index_.erase(resting.id);
                    level.pop_front();
                }
            }

            if (level.empty()) asks_.erase(levelIt);
        }

        if (order.quantity > 0) {
            index_[order.id] = OrderLocation{Side::Buy, order.price};
            bids_[order.price].push_back(order);
        }
    } else {
        while (order.quantity > 0 && !bids_.empty() && bids_.begin()->first >= order.price) {
            auto levelIt = bids_.begin();
            std::deque<Order>& level = levelIt->second;

            while (order.quantity > 0 && !level.empty()) {
                Order& resting = level.front();
                std::int64_t filled = std::min(order.quantity, resting.quantity);

                trades.push_back(Trade{resting.id, order.id, resting.price, filled});
                order.quantity -= filled;
                resting.quantity -= filled;

                if (resting.quantity == 0) {
                    index_.erase(resting.id);
                    level.pop_front();
                }
            }

            if (level.empty()) bids_.erase(levelIt);
        }

        if (order.quantity > 0) {
            index_[order.id] = OrderLocation{Side::Sell, order.price};
            asks_[order.price].push_back(order);
        }
    }

    return trades;
}

bool OrderBook::cancelOrder(std::int64_t orderId) {
    auto idxIt = index_.find(orderId);
    if (idxIt == index_.end()) return false;

    OrderLocation loc = idxIt->second;
    auto eraseFrom = [&](auto& book) {
        auto levelIt = book.find(loc.price);
        if (levelIt == book.end()) return false;
        auto& level = levelIt->second;
        for (auto it = level.begin(); it != level.end(); ++it) {
            if (it->id == orderId) {
                level.erase(it);
                if (level.empty()) book.erase(levelIt);
                return true;
            }
        }
        return false;
    };

    bool erased = loc.side == Side::Buy ? eraseFrom(bids_) : eraseFrom(asks_);
    index_.erase(idxIt);
    return erased;
}

std::optional<double> OrderBook::bestBid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<double> OrderBook::bestAsk() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

template <typename Book>
std::vector<std::pair<double, std::int64_t>> OrderBook::depthOf(const Book& book, std::size_t depth) const {
    std::vector<std::pair<double, std::int64_t>> result;
    result.reserve(depth);
    for (auto it = book.begin(); it != book.end() && result.size() < depth; ++it) {
        std::int64_t total = 0;
        for (const Order& o : it->second) total += o.quantity;
        result.emplace_back(it->first, total);
    }
    return result;
}

std::vector<std::pair<double, std::int64_t>> OrderBook::bidDepth(std::size_t depth) const {
    return depthOf(bids_, depth);
}

std::vector<std::pair<double, std::int64_t>> OrderBook::askDepth(std::size_t depth) const {
    return depthOf(asks_, depth);
}

} // namespace matching
