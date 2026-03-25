#include "udp_receiver.hpp"
#include "packet.hpp"
#include "../utils/logger.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include <string>
#include <chrono>

// Max UDP payload: 256 samples * 2 bytes + 16 byte header
static constexpr size_t MAX_UDP_PACKET = HEADER_SIZE + 256 * sizeof(int16_t);

// If no packet received for this long, mark client as inactive
static constexpr int TIMEOUT_MS = 2000;

UdpReceiver::UdpReceiver(uint16_t port, RingBuffer& dest)
    : port_(port), dest_(dest) {}

UdpReceiver::~UdpReceiver() { stop(); }

void UdpReceiver::start() {
    running_ = true;
    thread_  = std::thread(&UdpReceiver::recv_loop, this);
}

void UdpReceiver::stop() {
    running_ = false;
    if (sock_ != static_cast<uintptr_t>(-1)) {
        closesocket(static_cast<SOCKET>(sock_));
        sock_ = static_cast<uintptr_t>(-1);
    }
    if (thread_.joinable()) thread_.join();
}

void UdpReceiver::recv_loop() {
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sock_ = static_cast<uintptr_t>(s);

    // Socket receive timeout so we can check running_ periodically
    DWORD timeout_ms = 500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LOG_ERR("UDP bind failed on port " + std::to_string(port_));
        WSACleanup();
        return;
    }

    LOG_INFO("UDP receiver listening on port " + std::to_string(port_));

    uint8_t buf[MAX_UDP_PACKET];
    auto last_packet = std::chrono::steady_clock::now();

    while (running_) {
        int received = recvfrom(s, reinterpret_cast<char*>(buf),
                                sizeof(buf), 0, nullptr, nullptr);

        if (received <= 0) {
            // Check timeout — mark inactive if phone went silent
            auto now = std::chrono::steady_clock::now();
            int elapsed = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - last_packet).count());
            if (client_active_ && elapsed > TIMEOUT_MS) {
                client_active_ = false;
                LOG_INFO("Phone went silent");
            }
            continue;
        }

        if (static_cast<size_t>(received) < HEADER_SIZE) continue;

        const auto* hdr = reinterpret_cast<const AudioPacketHeader*>(buf);
        if (ntohl(hdr->magic) != PACKET_MAGIC) continue;

        uint32_t payload_sz = ntohl(hdr->payload_size);
        if (payload_sz == 0 || static_cast<size_t>(received) < HEADER_SIZE + payload_sz) continue;

        const int16_t* raw_samples = reinterpret_cast<const int16_t*>(buf + HEADER_SIZE);
        size_t sample_count = payload_sz / sizeof(int16_t);

        // Phone sends big-endian samples; convert to little-endian (host order)
        int16_t swapped[256];
        size_t to_swap = (sample_count > 256) ? 256 : sample_count;
        for (size_t i = 0; i < to_swap; ++i)
            swapped[i] = static_cast<int16_t>(ntohs(static_cast<uint16_t>(raw_samples[i])));

        dest_.write(swapped, to_swap);

        if (!client_active_) {
            client_active_ = true;
            LOG_INFO("Phone connected (UDP)");
        }
        last_packet = std::chrono::steady_clock::now();
    }

    WSACleanup();
}
