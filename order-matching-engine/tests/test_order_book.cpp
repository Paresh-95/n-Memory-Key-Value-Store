// Lightweight assert-based test suite (no external framework dependency).
#include <cassert>
#include <iostream>

#include "OrderBook.hpp"

using namespace matching;

namespace {

Order makeOrder(std::int64_t id, Side side, double price, std::int64_t qty) {
    Order o;
    o.id = id;
    o.side = side;
    o.price = price;
    o.quantity = qty;
    o.timestamp = std::chrono::steady_clock::now();
    return o;
}

void testRestingOrderNoMatch() {
    OrderBook book;
    auto trades = book.addOrder(makeOrder(1, Side::Buy, 100.0, 10));
    assert(trades.empty());
    assert(book.bestBid().value() == 100.0);
    assert(!book.bestAsk().has_value());

    std::cout << "[PASS] testRestingOrderNoMatch" << std::endl;
}

void testFullMatch() {
    OrderBook book;
    book.addOrder(makeOrder(1, Side::Sell, 100.0, 10));
    auto trades = book.addOrder(makeOrder(2, Side::Buy, 100.0, 10));

    assert(trades.size() == 1);
    assert(trades[0].buyOrderId == 2);
    assert(trades[0].sellOrderId == 1);
    assert(trades[0].price == 100.0);
    assert(trades[0].quantity == 10);
    assert(!book.bestAsk().has_value());
    assert(!book.bestBid().has_value());

    std::cout << "[PASS] testFullMatch" << std::endl;
}

void testPartialFill() {
    OrderBook book;
    book.addOrder(makeOrder(1, Side::Sell, 100.0, 5));
    auto trades = book.addOrder(makeOrder(2, Side::Buy, 100.0, 10));

    assert(trades.size() == 1);
    assert(trades[0].quantity == 5);
    assert(!book.bestAsk().has_value());
    // Buy order had 10, 5 filled, 5 rests on the book.
    assert(book.bestBid().value() == 100.0);
    auto depth = book.bidDepth(1);
    assert(depth.size() == 1 && depth[0].second == 5);

    std::cout << "[PASS] testPartialFill" << std::endl;
}

void testPricePriority() {
    OrderBook book;
    // Two resting sells; the cheaper one should match first even though
    // it was placed second.
    book.addOrder(makeOrder(1, Side::Sell, 101.0, 10));
    book.addOrder(makeOrder(2, Side::Sell, 100.0, 10));

    auto trades = book.addOrder(makeOrder(3, Side::Buy, 101.0, 10));
    assert(trades.size() == 1);
    assert(trades[0].sellOrderId == 2);
    assert(trades[0].price == 100.0);

    std::cout << "[PASS] testPricePriority" << std::endl;
}

void testTimePriority() {
    OrderBook book;
    // Two resting sells at the same price; the earlier one (id=1) must
    // fill first (FIFO within a price level).
    book.addOrder(makeOrder(1, Side::Sell, 100.0, 5));
    book.addOrder(makeOrder(2, Side::Sell, 100.0, 5));

    auto trades = book.addOrder(makeOrder(3, Side::Buy, 100.0, 5));
    assert(trades.size() == 1);
    assert(trades[0].sellOrderId == 1);

    auto depth = book.askDepth(1);
    assert(depth.size() == 1 && depth[0].second == 5); // order 2 still resting

    std::cout << "[PASS] testTimePriority" << std::endl;
}

void testCancel() {
    OrderBook book;
    book.addOrder(makeOrder(1, Side::Buy, 100.0, 10));
    assert(book.cancelOrder(1));
    assert(!book.bestBid().has_value());
    assert(!book.cancelOrder(1)); // already gone

    // A cancelled order must not participate in matching.
    book.addOrder(makeOrder(2, Side::Buy, 100.0, 10));
    book.cancelOrder(2);
    auto trades = book.addOrder(makeOrder(3, Side::Sell, 100.0, 10));
    assert(trades.empty());

    std::cout << "[PASS] testCancel" << std::endl;
}

void testInvalidInputThrows() {
    OrderBook book;
    bool threw = false;
    try {
        book.addOrder(makeOrder(1, Side::Buy, -1.0, 10));
    } catch (const OrderError&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        book.addOrder(makeOrder(2, Side::Buy, 100.0, 0));
    } catch (const OrderError&) {
        threw = true;
    }
    assert(threw);

    std::cout << "[PASS] testInvalidInputThrows" << std::endl;
}

} // namespace

int main() {
    testRestingOrderNoMatch();
    testFullMatch();
    testPartialFill();
    testPricePriority();
    testTimePriority();
    testCancel();
    testInvalidInputThrows();

    std::cout << "All tests passed." << std::endl;
    return 0;
}
