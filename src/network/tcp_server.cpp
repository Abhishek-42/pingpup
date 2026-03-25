#include "tcp_server.hpp"
#include "packet.hpp"
#include "../utils/logger.hpp"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include <vector>
#include <string>

TcpServer::TcpServer(uint16_t port, RingBuffer& dest)
    : port_(port), dest_(dest) {}

TcpServer::~TcpServer() { stop(); }

void TcpServer::start() {
    running_ = true;
    thread_  = std::thread(&TcpServer::listen_loop, this);
}

void TcpServer::stop() {
    running_ = false;
    if (server_sock_ != static_cast<uintptr_t>(-1)) {
        closesocket(static_cast<SOCKET>(server_sock_));
        server_sock_ = static_cast<uintptr_t>(-1);
    }
    if (thread_.joinable()) thread_.join();
}

bool TcpServer::recv_exact(uintptr_t sock, uint8_t* buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        int r = recv(static_cast<SOCKET>(sock),
                     reinterpret_cast<char*>(buf + received),
                     static_cast<int>(len - received), 0);
        if (r <= 0) return false;
        received += r;
    }
    return true;
}

void TcpServer::listen_loop() {
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    server_sock_ = static_cast<uintptr_t>(srv);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        LOG_ERR("TCP bind failed on port " + std::to_string(port_));
        WSACleanup();
        return;
    }
    listen(srv, 1);
    LOG_INFO("TCP server listening on port " + std::to_string(port_));

    while (running_) {
        SOCKET client = accept(srv, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;

        client_connected_ = true;
        LOG_INFO("Phone connected");

        AudioPacketHeader hdr{};
        std::vector<uint8_t> payload;

        while (running_) {
            if (!recv_exact(static_cast<uintptr_t>(client),
                            reinterpret_cast<uint8_t*>(&hdr), HEADER_SIZE)) break;

            if (hdr.magic != PACKET_MAGIC) {
                LOG_WARN("Bad packet magic — dropping connection");
                break;
            }

            uint32_t sz = ntohl(hdr.payload_size);
            if (sz == 0 || sz > 65536) { LOG_WARN("Suspicious payload size"); break; }

            payload.resize(sz);
            if (!recv_exact(static_cast<uintptr_t>(client), payload.data(), sz)) break;

            size_t samples = sz / sizeof(int16_t);
            dest_.write(reinterpret_cast<const int16_t*>(payload.data()), samples);
        }

        closesocket(client);
        client_connected_ = false;
        LOG_INFO("Phone disconnected");
    }

    WSACleanup();
}
