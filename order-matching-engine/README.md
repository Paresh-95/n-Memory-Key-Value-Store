# Order Matching Engine

A simplified, low-latency order matching engine in C++17, using price-time
priority matching (like a stock exchange's limit order book).

## Features
- Limit order submission (`BUY`/`SELL`) with price-time priority matching
- Partial fills, order cancellation
- Single-threaded matching core fed by a thread-safe queue, so multiple
  producer threads can submit orders concurrently without the order book
  itself needing any locking
- Input validation via exceptions, thread-safe logging of trades
- Throughput benchmark tool

## Design

**Order book** (`OrderBook`): two `std::map`s (bids sorted highest-price-first,
asks sorted lowest-price-first) where each price level holds a `std::deque`
of resting orders. This gives:
- **Price priority** for free — `std::map` keeps price levels sorted.
- **Time priority** for free — a `deque`'s front is the oldest order at that
  price, so matching pulls FIFO.
- **O(1) cancellation** via an `unordered_map<orderId, {side, price}>` index
  that locates the right price level directly, instead of scanning the book.

A `std::priority_queue` was deliberately *not* used for the live book: it
has no efficient way to remove an arbitrary element, which cancellation
needs. `map` + `deque` gives sorted access, FIFO ordering, and cheap removal
all at once.

**Concurrency model**: real exchanges keep the matching engine for a given
symbol single-threaded, because concurrent matching against the same book is
a correctness minefield (two threads could both think they're matching
against the same resting order). This project follows that: `OrderBook` and
`MatchingEngine` are not internally synchronized at all. Instead, a
thread-safe `OrderQueue<T>` sits at the boundary — any number of producer
threads can push orders/cancels, and exactly one matching thread drains the
queue and applies them one at a time.

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

Produces:
- `matching_engine` — interactive REPL
- `matching_tests` — test suite
- `matching_bench` — throughput benchmark

## Running

```bash
./build/matching_engine
```

```
Order matching engine ready. Commands:
  BUY price qty
  SELL price qty
  CANCEL id
  BOOK
  QUIT
SELL 100.5 10
Order #1 submitted (resting)
BUY 100.5 4
Order #2 submitted (1 trade(s))
BOOK
-- Book --
Asks (best first):
  100.5 x 6
Bids (best first):
CANCEL 1
Cancelled #1
QUIT
Shut down cleanly.
```

## Testing

```bash
cd build && ctest --output-on-failure
```

Covers: resting orders with no match, full match, partial fill, price
priority (better price fills first), time priority (FIFO within a price
level), cancellation (including that a cancelled order can't be matched
later), and rejection of invalid input (non-positive price/quantity).

## Benchmarking

```bash
./build/matching_bench [num_orders]
```

Feeds a stream of random buy/sell orders through a single `OrderBook` and
reports orders/sec processed.

## Concepts Demonstrated
- STL containers: `map`, `unordered_map`, `deque`, `queue`
- Price-time priority matching algorithm
- Multithreading via a producer/single-consumer thread-safe queue
- Exception-based validation (`OrderError`)
- Thread-safe logging
- Assert-based testing and throughput benchmarking
