#pragma once
#include <thread>
#include <atomic>
#include "../audio/ring_buffer.hpp"

// Continuously reads from two source RingBuffers, applies per-source volume,
// sums the samples (with clipping), and writes into the output RingBuffer.
class Mixer {
public:
    Mixer(RingBuffer& phone_src,
          RingBuffer& system_src,
          RingBuffer& output,
          int         frames_per_chunk = 512);
    ~Mixer();

    void start();
    void stop();

    void set_phone_volume(float v)  { phone_vol_.store(v);  }
    void set_system_volume(float v) { system_vol_.store(v); }

private:
    void mix_loop();

    RingBuffer&        phone_src_;
    RingBuffer&        system_src_;
    RingBuffer&        output_;
    int                chunk_;
    std::atomic<float> phone_vol_{1.0f};
    std::atomic<float> system_vol_{1.0f};
    std::thread        thread_;
    std::atomic<bool>  running_{false};
};
