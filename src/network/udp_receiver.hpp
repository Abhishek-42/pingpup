#pragma once
#include <cstdint>
#include <thread>
#include <atomic>
#include "../audio/ring_buffer.hpp"

// Receives UDP audio packets from the Android app and pushes PCM samples into a RingBuffer.
// No connection needed — phone just fires packets at PC_IP:9000.
class UdpReceiver {
public:
    UdpReceiver(uint16_t port, RingBuffer& dest);
    ~UdpReceiver();

    void start();
    void stop();

    bool client_active() const { return client_active_; }

private:
    void recv_loop();

    uint16_t           port_;
    RingBuffer&        dest_;
    std::thread        thread_;
    std::atomic<bool>  running_{false};
    std::atomic<bool>  client_active_{false};
    uintptr_t          sock_{static_cast<uintptr_t>(-1)};
};
