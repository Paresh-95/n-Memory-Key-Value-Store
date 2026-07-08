# In-Memory Key-Value Store

A Redis-inspired in-memory key-value store built in C++17. It supports CRUD
operations, configurable key expiration (TTL), thread-safe concurrent access,
and is exposed over a simple TCP protocol so multiple clients can connect at
once.

> This repository also contains a second, independent project: an
> [order matching engine](order-matching-engine/) with price-time priority
> matching. See its own README for details.

## Features
- CRUD operations (`SET`, `GET`, `DEL`, plus `EXISTS`/`TTL`/`EXPIRE`)
- Configurable per-key expiration (TTL) with lazy expiry on access and a
  background reaper thread that sweeps expired keys periodically
- Thread-safe concurrent access via `std::shared_mutex` (concurrent reads,
  exclusive writes) so many client connections can hit the store at once
- `unordered_map`-backed index for average O(1) lookups, with an optional
  bounded-capacity LRU eviction policy (doubly-linked list + hash map)
- Multithreaded TCP server: one thread per client connection
- Benchmark tool for measuring concurrent throughput

## Tech Stack
- C++17, STL
- Multithreading, mutexes (`std::shared_mutex`), condition variables
- `unordered_map` + intrusive LRU list for caching/eviction
- POSIX sockets for the TCP server
- CMake

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

This produces three binaries in `build/`:
- `kvstore_server` — the TCP server
- `kvstore_tests` — the test suite
- `kvstore_bench` — a concurrent throughput benchmark

## Running the server

```bash
./build/kvstore_server [port] [max_capacity]
# e.g.
./build/kvstore_server 6380 10000
```

`port` defaults to `6380`. `max_capacity` defaults to `0` (unbounded); when
set to a positive value, the store evicts the least-recently-used key once
that many entries are stored.

Connect with any TCP client, e.g. `nc` or `telnet`:

```bash
nc 127.0.0.1 6380
SET user:1 Paresh
OK
GET user:1
VALUE Paresh
SET session:1 abc123 5000
OK
TTL session:1
4991
EXISTS user:1
TRUE
DEL user:1
DELETED
GET user:1
NOT_FOUND
QUIT
BYE
```

## Supported Commands

| Command                  | Description                                  | Response                  |
|---------------------------|-----------------------------------------------|----------------------------|
| `SET key value [ttl_ms]`  | Insert/overwrite a key, optional TTL in ms    | `OK`                       |
| `GET key`                  | Fetch a value                                 | `VALUE <v>` / `NOT_FOUND`  |
| `DEL key`                  | Remove a key                                  | `DELETED` / `NOT_FOUND`    |
| `EXISTS key`               | Check whether a (non-expired) key exists      | `TRUE` / `FALSE`           |
| `TTL key`                  | Remaining TTL in ms                           | `<ms>` / `NONE` / `NOT_FOUND` |
| `EXPIRE key ttl_ms`        | Set/replace TTL on an existing key            | `OK` / `NOT_FOUND`         |
| `PING`                     | Liveness check                                | `PONG`                     |
| `QUIT`                     | Close the connection                          | `BYE`                      |

## Testing

```bash
cd build && ctest --output-on-failure
# or directly:
./build/kvstore_tests
```

The suite covers CRUD correctness, TTL expiration (lazy and via the reaper
thread), LRU eviction, and concurrent access from multiple threads.

## Benchmarking

```bash
./build/kvstore_bench [threads] [ops_per_thread]
```

Runs concurrent `SET`/`GET` pairs across the given number of threads and
reports total throughput (ops/sec), useful for evaluating the impact of
locking strategy and capacity/eviction settings.

## Design Notes

- **Locking**: a single `std::shared_mutex` guards the map and LRU list.
  Reads that don't mutate LRU order (`exists`) take a shared lock; `get`
  takes a unique lock since it must update LRU position and may lazily
  evict an expired entry.
- **Expiration**: entries store an optional `steady_clock` expiry time.
  Expired entries are removed either lazily (on next access) or by a
  background thread that wakes on a timer via `condition_variable_any`
  and exits cleanly on shutdown.
- **Eviction**: when a capacity is configured, a doubly linked list tracks
  recency of use; the map stores an iterator into that list per entry so
  both lookup and LRU promotion/eviction are O(1) on average.
- **Concurrency model**: the server accepts connections on the main thread
  and spawns one worker thread per client, all sharing a single `KVStore`
  instance — demonstrating multithreaded access to a shared, lock-protected
  resource rather than one store per connection.

## Concepts Demonstrated
- Object-Oriented Programming and RAII (background thread lifecycle tied to
  `KVStore` construction/destruction)
- STL containers (`unordered_map`, `list`) and standard synchronization
  primitives (`shared_mutex`, `condition_variable_any`, `atomic`)
- Multithreading and concurrency control
- Systems programming with POSIX sockets
- Basic performance profiling via the bundled benchmark tool
