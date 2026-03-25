#pragma once
#include <atomic>
#include <portaudio.h>
#include "../audio/ring_buffer.hpp"

// Pulls mixed PCM int16 mono samples from a RingBuffer and plays them via PortAudio.
class PortAudioOutput {
public:
    PortAudioOutput(RingBuffer& src, int sample_rate = 44100, int frames_per_buf = 512);
    ~PortAudioOutput();

    bool start();
    void stop();

private:
    static int pa_callback(const void* in, void* out,
                           unsigned long frames,
                           const PaStreamCallbackTimeInfo* time_info,
                           PaStreamCallbackFlags status_flags,
                           void* user_data);

    RingBuffer&       src_;
    int               sample_rate_;
    int               frames_per_buf_;
    void*             stream_{nullptr};   // PaStream*
    std::atomic<bool> running_{false};
};
