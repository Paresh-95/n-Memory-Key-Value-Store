#include "Server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace kvstore {

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

} // namespace

Server::Server(KVStore& store, uint16_t port) : store_(store), port_(port) {}

Server::~Server() {
    stop();
}

void Server::run() {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        throw std::runtime_error("failed to create socket");
    }

    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(listenFd_);
        throw std::runtime_error("failed to bind to port " + std::to_string(port_));
    }

    if (listen(listenFd_, /*backlog=*/64) < 0) {
        close(listenFd_);
        throw std::runtime_error("failed to listen on socket");
    }

    running_ = true;
    std::cout << "KVStore server listening on port " << port_ << std::endl;

    while (running_) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            if (!running_) break;
            continue;
        }
        workers_.emplace_back(&Server::handleClient, this, clientFd);
    }

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

void Server::stop() {
    if (!running_) return;
    running_ = false;
    if (listenFd_ >= 0) {
        shutdown(listenFd_, SHUT_RDWR);
        close(listenFd_);
        listenFd_ = -1;
    }
}

void Server::handleClient(int clientFd) {
    std::string buffer;
    char chunk[4096];

    while (running_) {
        ssize_t n = recv(clientFd, chunk, sizeof(chunk), 0);
        if (n <= 0) break;
        buffer.append(chunk, static_cast<std::size_t>(n));

        std::size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            buffer.erase(0, pos + 1);

            if (line.empty()) continue;
            std::string response = dispatch(line);
            response.push_back('\n');
            send(clientFd, response.data(), response.size(), 0);

            if (line == "QUIT" || line == "quit") {
                close(clientFd);
                return;
            }
        }
    }
    close(clientFd);
}

std::string Server::dispatch(const std::string& line) {
    auto tokens = split(line);
    if (tokens.empty()) return "ERROR empty command";

    std::string cmd = tokens[0];
    for (auto& c : cmd) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));

    try {
        if (cmd == "PING") {
            return "PONG";
        } else if (cmd == "SET") {
            if (tokens.size() < 3) return "ERROR usage: SET key value [ttl_ms]";
            std::string key = tokens[1];
            std::string value = tokens[2];
            std::chrono::milliseconds ttl(0);
            if (tokens.size() >= 4) {
                ttl = std::chrono::milliseconds(std::stoll(tokens[3]));
            }
            store_.set(key, value, ttl);
            return "OK";
        } else if (cmd == "GET") {
            if (tokens.size() != 2) return "ERROR usage: GET key";
            auto val = store_.get(tokens[1]);
            return val.has_value() ? "VALUE " + *val : "NOT_FOUND";
        } else if (cmd == "DEL") {
            if (tokens.size() != 2) return "ERROR usage: DEL key";
            return store_.del(tokens[1]) ? "DELETED" : "NOT_FOUND";
        } else if (cmd == "EXISTS") {
            if (tokens.size() != 2) return "ERROR usage: EXISTS key";
            return store_.exists(tokens[1]) ? "TRUE" : "FALSE";
        } else if (cmd == "TTL") {
            if (tokens.size() != 2) return "ERROR usage: TTL key";
            auto t = store_.ttl(tokens[1]);
            if (!t.has_value()) return "NOT_FOUND";
            if (t->count() == 0) return "NONE";
            return std::to_string(t->count());
        } else if (cmd == "EXPIRE") {
            if (tokens.size() != 3) return "ERROR usage: EXPIRE key ttl_ms";
            bool ok = store_.expire(tokens[1], std::chrono::milliseconds(std::stoll(tokens[2])));
            return ok ? "OK" : "NOT_FOUND";
        } else if (cmd == "QUIT") {
            return "BYE";
        }
    } catch (const std::exception& e) {
        return std::string("ERROR ") + e.what();
    }

    return "ERROR unknown command";
}

} // namespace kvstore
