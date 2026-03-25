#pragma once
#include <thread>
#include <atomic>
#include "../audio/ring_buffer.hpp"

// Captures system audio (loopback) via WASAPI and pushes PCM int16 mono samples
// into the provided RingBuffer at 44100 Hz.
class WasapiCapture {
public:
    explicit WasapiCapture(RingBuffer& dest);
    ~WasapiCapture();

    bool start();
    void stop();

private:
    void capture_loop();

    RingBuffer&        dest_;
    std::thread        thread_;
    std::atomic<bool>  running_{false};
};
