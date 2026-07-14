#include <csignal>
#include <cstdlib>
#include <iostream>

#include "KVStore.hpp"
#include "Server.hpp"

namespace {
kvstore::Server* g_server = nullptr;

void handleSignal(int) {
    if (g_server) g_server->stop();
}
} // namespace

int main(int argc, char** argv) {
    uint16_t port = 6380;
    std::size_t capacity = 0;

    if (argc >= 2) port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (argc >= 3) capacity = static_cast<std::size_t>(std::atoll(argv[2]));

    kvstore::KVStore store(capacity);
    kvstore::Server server(store, port);
    g_server = &server;

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    try {
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Server shut down cleanly." << std::endl;
    return 0;
}
