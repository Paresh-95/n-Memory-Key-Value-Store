#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace matching {

enum class LogLevel { Info, Warn, Error };

// Minimal thread-safe logger: multiple threads (the input thread and the
// matching thread) can log concurrently without interleaving output.
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "[" << timestamp() << "] [" << levelName(level) << "] "
                   << message << std::endl;
    }

private:
    static std::string levelName(LogLevel level) {
        switch (level) {
            case LogLevel::Info: return "INFO";
            case LogLevel::Warn: return "WARN";
            case LogLevel::Error: return "ERROR";
        }
        return "UNKNOWN";
    }

    static std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&t, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }

    std::mutex mutex_;
};

inline void logInfo(const std::string& message) { Logger::instance().log(LogLevel::Info, message); }
inline void logWarn(const std::string& message) { Logger::instance().log(LogLevel::Warn, message); }
inline void logError(const std::string& message) { Logger::instance().log(LogLevel::Error, message); }

} // namespace matching
