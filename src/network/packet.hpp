#pragma once
#include <cstdint>

// Every audio chunk sent from the phone is prefixed with this header (big-endian on wire)
#pragma pack(push, 1)
struct AudioPacketHeader {
    uint32_t magic;        // 0xABCD1234 — sanity check
    uint32_t payload_size; // bytes that follow (PCM int16 samples)
    uint64_t timestamp_ms; // sender-side timestamp (optional, for jitter stats)
};
#pragma pack(pop)

static constexpr uint32_t PACKET_MAGIC = 0xABCD1234;
static constexpr size_t   HEADER_SIZE  = sizeof(AudioPacketHeader);
