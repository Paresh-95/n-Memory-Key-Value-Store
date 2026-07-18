#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Logger.hpp"
#include "MatchingEngine.hpp"
#include "OrderQueue.hpp"

namespace {

using namespace matching;

// A queued command carries what the matcher thread needs to execute it and
// print a result -- either a new order (side/price/qty) or a cancel.
struct Command {
    enum class Type { NewOrder, Cancel, PrintBook } type;
    Side side = Side::Buy;
    double price = 0.0;
    std::int64_t quantity = 0;
    std::int64_t cancelId = 0;
};

void printBook(const OrderBook& book) {
    std::cout << "-- Book --\n";
    std::cout << "Asks (best first):\n";
    auto asks = book.askDepth(5);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << "  " << it->first << " x " << it->second << "\n";
    }
    std::cout << "Bids (best first):\n";
    for (const auto& [price, qty] : book.bidDepth(5)) {
        std::cout << "  " << price << " x " << qty << "\n";
    }
}

// Runs on the single matching thread: pulls commands off the queue one at a
// time and applies them to the engine. This is the only thread that ever
// touches MatchingEngine/OrderBook, so no locking is needed inside them.
void matcherLoop(OrderQueue<Command>& queue, MatchingEngine& engine, std::atomic<bool>& done) {
    while (auto cmdOpt = queue.pop()) {
        const Command& cmd = *cmdOpt;
        try {
            if (cmd.type == Command::Type::NewOrder) {
                std::vector<Trade> trades;
                std::int64_t id = engine.submit(cmd.side, cmd.price, cmd.quantity, trades);
                std::cout << "Order #" << id << " submitted ("
                          << (trades.empty() ? "resting" : std::to_string(trades.size()) + " trade(s)")
                          << ")\n";
            } else if (cmd.type == Command::Type::Cancel) {
                bool ok = engine.cancel(cmd.cancelId);
                std::cout << (ok ? "Cancelled" : "Cancel failed (unknown id)") << " #" << cmd.cancelId << "\n";
            } else if (cmd.type == Command::Type::PrintBook) {
                printBook(engine.book());
            }
        } catch (const OrderError& e) {
            logError(std::string("rejected: ") + e.what());
        }
    }
    done = true;
}

} // namespace

int main() {
    MatchingEngine engine;
    OrderQueue<Command> queue;
    std::atomic<bool> matcherDone{false};

    std::thread matcher(matcherLoop, std::ref(queue), std::ref(engine), std::ref(matcherDone));

    std::cout << "Order matching engine ready. Commands:\n"
              << "  BUY price qty\n  SELL price qty\n  CANCEL id\n  BOOK\n  QUIT\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string verb;
        iss >> verb;
        for (auto& c : verb) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));

        if (verb == "BUY" || verb == "SELL") {
            double price;
            std::int64_t qty;
            if (!(iss >> price >> qty)) {
                std::cout << "usage: " << verb << " price qty\n";
                continue;
            }
            Command cmd{Command::Type::NewOrder};
            cmd.side = (verb == "BUY") ? Side::Buy : Side::Sell;
            cmd.price = price;
            cmd.quantity = qty;
            queue.push(cmd);
        } else if (verb == "CANCEL") {
            std::int64_t id;
            if (!(iss >> id)) {
                std::cout << "usage: CANCEL id\n";
                continue;
            }
            Command cmd{Command::Type::Cancel};
            cmd.cancelId = id;
            queue.push(cmd);
        } else if (verb == "BOOK") {
            queue.push(Command{Command::Type::PrintBook});
        } else if (verb == "QUIT") {
            break;
        } else if (!verb.empty()) {
            std::cout << "unknown command: " << verb << "\n";
        }
    }

    queue.stop();
    matcher.join();
    std::cout << "Shut down cleanly.\n";
    return 0;
}
