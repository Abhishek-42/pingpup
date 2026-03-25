#pragma once
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

enum class LogLevel { INFO, WARN, ERR };

inline std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");
    return ss.str();
}

inline void log(LogLevel level, const std::string& msg) {
    const char* tag = (level == LogLevel::INFO) ? "INFO"
                    : (level == LogLevel::WARN) ? "WARN" : "ERR ";
    std::cout << "[" << timestamp() << "][" << tag << "] " << msg << "\n";
}

#define LOG_INFO(msg) log(LogLevel::INFO, msg)
#define LOG_WARN(msg) log(LogLevel::WARN, msg)
#define LOG_ERR(msg)  log(LogLevel::ERR,  msg)
