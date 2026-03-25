#pragma once
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>
#include "../audio/ring_buffer.hpp"

// Listens on a TCP port, receives AudioPacketHeader+PCM chunks, pushes samples into a RingBuffer.
class TcpServer {
public:
    TcpServer(uint16_t port, RingBuffer& dest);
    ~TcpServer();

    void start();   // non-blocking — spawns listener thread
    void stop();

    bool client_connected() const { return client_connected_; }

private:
    void listen_loop();
    bool recv_exact(uintptr_t sock, uint8_t* buf, size_t len);

    uint16_t            port_;
    RingBuffer&         dest_;
    std::thread         thread_;
    std::atomic<bool>   running_{false};
    std::atomic<bool>   client_connected_{false};
    uintptr_t           server_sock_{static_cast<uintptr_t>(-1)};
};
