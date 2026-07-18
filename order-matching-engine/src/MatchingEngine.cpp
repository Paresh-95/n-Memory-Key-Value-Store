#include "MatchingEngine.hpp"

#include "Logger.hpp"

namespace matching {

std::int64_t MatchingEngine::submit(Side side, double price, std::int64_t quantity,
                                      std::vector<Trade>& outTrades) {
    Order order;
    order.id = nextOrderId_.fetch_add(1);
    order.side = side;
    order.price = price;
    order.quantity = quantity;
    order.timestamp = std::chrono::steady_clock::now();

    outTrades = book_.addOrder(order);

    for (const Trade& trade : outTrades) {
        logInfo("TRADE buy=" + std::to_string(trade.buyOrderId) +
                " sell=" + std::to_string(trade.sellOrderId) +
                " price=" + std::to_string(trade.price) +
                " qty=" + std::to_string(trade.quantity));
    }

    return order.id;
}

bool MatchingEngine::cancel(std::int64_t orderId) {
    bool ok = book_.cancelOrder(orderId);
    if (!ok) logWarn("cancel failed, unknown order id=" + std::to_string(orderId));
    return ok;
}

} // namespace matching
