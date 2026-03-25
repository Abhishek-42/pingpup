#include "mixer.hpp"
#include "../utils/logger.hpp"
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

Mixer::Mixer(RingBuffer& phone_src, RingBuffer& system_src,
             RingBuffer& output, int frames_per_chunk)
    : phone_src_(phone_src), system_src_(system_src),
      output_(output), chunk_(frames_per_chunk) {}

Mixer::~Mixer() { stop(); }

void Mixer::start() {
    running_ = true;
    thread_  = std::thread(&Mixer::mix_loop, this);
    LOG_INFO("Mixer started");
}

void Mixer::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    LOG_INFO("Mixer stopped");
}

void Mixer::mix_loop() {
    std::vector<int16_t> phone_buf(chunk_, 0);
    std::vector<int16_t> sys_buf(chunk_, 0);
    std::vector<int16_t> out_buf(chunk_);

    while (running_) {
        // Wait until at least one source has data
        if (phone_src_.available() < static_cast<size_t>(chunk_) &&
            system_src_.available() < static_cast<size_t>(chunk_)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        size_t phone_got = phone_src_.read(phone_buf.data(), chunk_);
        size_t sys_got   = system_src_.read(sys_buf.data(), chunk_);

        // Zero-pad if a source had less data
        for (size_t i = phone_got; i < static_cast<size_t>(chunk_); ++i) phone_buf[i] = 0;
        for (size_t i = sys_got;   i < static_cast<size_t>(chunk_); ++i) sys_buf[i]   = 0;

        float pv = phone_vol_.load();
        float sv = system_vol_.load();

        for (int i = 0; i < chunk_; ++i) {
            float mixed = phone_buf[i] * pv + sys_buf[i] * sv;
            // Soft clip
            mixed = std::max(-32768.0f, std::min(32767.0f, mixed));
            out_buf[i] = static_cast<int16_t>(mixed);
        }

        output_.write(out_buf.data(), chunk_);
    }
}
