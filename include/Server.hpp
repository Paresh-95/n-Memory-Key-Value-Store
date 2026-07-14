#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "KVStore.hpp"

namespace kvstore {

// Minimal TCP server that exposes KVStore over a line-based text protocol.
// Each accepted connection is handled on its own thread; all threads share
// a single KVStore instance guarded internally by its own locking.
//
// Protocol (one command per line, response terminated by "\n"):
//   SET key value [ttl_ms]   -> OK
//   GET key                  -> VALUE <value> | NOT_FOUND
//   DEL key                  -> DELETED | NOT_FOUND
//   EXISTS key               -> TRUE | FALSE
//   TTL key                  -> <ms> | NONE | NOT_FOUND
//   EXPIRE key ttl_ms        -> OK | NOT_FOUND
//   PING                     -> PONG
//   QUIT                     -> BYE (closes connection)
class Server {
public:
    Server(KVStore& store, uint16_t port);
    ~Server();

    // Blocks the calling thread accepting connections until stop() is called.
    void run();
    void stop();

private:
    void handleClient(int clientFd);
    std::string dispatch(const std::string& line);

    KVStore& store_;
    uint16_t port_;
    int listenFd_ = -1;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;
};

} // namespace kvstore
