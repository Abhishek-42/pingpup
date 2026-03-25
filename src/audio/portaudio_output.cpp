#include "portaudio_output.hpp"
#include "../utils/logger.hpp"
#include <portaudio.h>
#include <cstring>

PortAudioOutput::PortAudioOutput(RingBuffer& src, int sample_rate, int frames_per_buf)
    : src_(src), sample_rate_(sample_rate), frames_per_buf_(frames_per_buf) {}

PortAudioOutput::~PortAudioOutput() { stop(); }

bool PortAudioOutput::start() {
    if (Pa_Initialize() != paNoError) {
        LOG_ERR("PortAudio init failed");
        return false;
    }

    PaStreamParameters out_params{};
    out_params.device = Pa_GetDefaultOutputDevice();
    if (out_params.device == paNoDevice) {
        LOG_ERR("No default output device found");
        Pa_Terminate();
        return false;
    }
    out_params.channelCount              = 1;
    out_params.sampleFormat              = paInt16;
    out_params.suggestedLatency          =
        Pa_GetDeviceInfo(out_params.device)->defaultLowOutputLatency;
    out_params.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream_, nullptr, &out_params,
                                sample_rate_, frames_per_buf_,
                                paClipOff, pa_callback, this);
    if (err != paNoError) {
        LOG_ERR(std::string("Pa_OpenStream: ") + Pa_GetErrorText(err));
        Pa_Terminate();
        return false;
    }

    running_ = true;
    Pa_StartStream(stream_);
    LOG_INFO("PortAudio output started");
    return true;
}

void PortAudioOutput::stop() {
    running_ = false;
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        Pa_Terminate();
        LOG_INFO("PortAudio output stopped");
    }
}

int PortAudioOutput::pa_callback(const void*, void* out,
                                  unsigned long frames,
                                  const PaStreamCallbackTimeInfo*,
                                  PaStreamCallbackFlags, void* user_data) {
    auto*    self    = static_cast<PortAudioOutput*>(user_data);
    int16_t* output  = static_cast<int16_t*>(out);
    size_t   got     = self->src_.read(output, frames);

    // Zero-fill if buffer underrun
    if (got < frames)
        std::memset(output + got, 0, (frames - got) * sizeof(int16_t));

    return self->running_ ? 0 /*paContinue*/ : 1 /*paComplete*/;
}
