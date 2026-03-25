#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <deque>
#include <mutex>

// Shared state between audio engine and GUI — all atomic, no locks needed for scalars
struct AppState {
    // Status
    std::atomic<bool>  phone_active{false};
    std::atomic<bool>  system_active{false};
    std::atomic<bool>  running{true};

    // Volumes (read by mixer, written by GUI)
    std::atomic<float> phone_vol{1.0f};
    std::atomic<float> system_vol{1.0f};

    // Live stats (written by engine, read by GUI)
    std::atomic<int>   phone_buf_pct{0};   // 0-100
    std::atomic<int>   system_buf_pct{0};
    std::atomic<int>   output_buf_pct{0};
    std::atomic<float> latency_ms{0.0f};

    // Log lines
    std::mutex              log_mutex;
    std::deque<std::string> log_lines;
    void push_log(const std::string& line) {
        std::lock_guard<std::mutex> lk(log_mutex);
        if (log_lines.size() > 64) log_lines.pop_front();
        log_lines.push_back(line);
    }
};

// Runs the ImGui Win32+DX11 window on the calling thread (must be main thread).
// Returns when user closes the window or state.running becomes false.
void run_gui(AppState& state);
