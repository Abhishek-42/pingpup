#include "wasapi_capture.hpp"
#include "../utils/logger.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <vector>
#include <cmath>

static constexpr int    TARGET_SAMPLE_RATE = 44100;
static constexpr int    TARGET_CHANNELS    = 1;       // mono

WasapiCapture::WasapiCapture(RingBuffer& dest) : dest_(dest) {}
WasapiCapture::~WasapiCapture() { stop(); }

bool WasapiCapture::start() {
    running_ = true;
    thread_  = std::thread(&WasapiCapture::capture_loop, this);
    return true;
}

void WasapiCapture::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}

void WasapiCapture::capture_loop() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice*           device     = nullptr;
    IAudioClient*        client     = nullptr;
    IAudioCaptureClient* capture    = nullptr;

    auto cleanup = [&]() {
        if (capture)    { capture->Release();    capture    = nullptr; }
        if (client)     { client->Release();     client     = nullptr; }
        if (device)     { device->Release();     device     = nullptr; }
        if (enumerator) { enumerator->Release(); enumerator = nullptr; }
        CoUninitialize();
    };

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) { LOG_ERR("WASAPI: CoCreateInstance failed"); cleanup(); return; }

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) { LOG_ERR("WASAPI: GetDefaultAudioEndpoint failed"); cleanup(); return; }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client));
    if (FAILED(hr)) { LOG_ERR("WASAPI: Activate failed"); cleanup(); return; }

    WAVEFORMATEX* fmt = nullptr;
    client->GetMixFormat(&fmt);

    // Initialize in loopback mode
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                            AUDCLNT_STREAMFLAGS_LOOPBACK,
                            10000000, 0, fmt, nullptr);
    if (FAILED(hr)) { LOG_ERR("WASAPI: Initialize failed"); CoTaskMemFree(fmt); cleanup(); return; }

    hr = client->GetService(__uuidof(IAudioCaptureClient),
                            reinterpret_cast<void**>(&capture));
    if (FAILED(hr)) { LOG_ERR("WASAPI: GetService failed"); CoTaskMemFree(fmt); cleanup(); return; }

    UINT32 src_rate     = fmt->nSamplesPerSec;
    UINT16 src_channels = fmt->nChannels;
    bool   is_float     = false;
    if (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        is_float = true;
    } else if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE && fmt->cbSize >= 22) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(fmt);
        is_float = (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }
    CoTaskMemFree(fmt);

    client->Start();
    LOG_INFO("WASAPI loopback capture started (" + std::to_string(src_rate) + " Hz, "
             + std::to_string(src_channels) + "ch)");

    std::vector<int16_t> mono_buf;

    while (running_) {
        UINT32 packet_size = 0;
        capture->GetNextPacketSize(&packet_size);

        while (packet_size > 0) {
            BYTE*  data   = nullptr;
            UINT32 frames = 0;
            DWORD  flags  = 0;
            capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && frames > 0) {
                mono_buf.resize(frames);
                for (UINT32 i = 0; i < frames; ++i) {
                    float sample = 0.0f;
                    if (is_float) {
                        const float* f = reinterpret_cast<const float*>(data);
                        for (UINT16 ch = 0; ch < src_channels; ++ch)
                            sample += f[i * src_channels + ch];
                        sample /= src_channels;
                    } else {
                        const int16_t* s = reinterpret_cast<const int16_t*>(data);
                        int32_t sum = 0;
                        for (UINT16 ch = 0; ch < src_channels; ++ch)
                            sum += s[i * src_channels + ch];
                        sample = static_cast<float>(sum) / src_channels / 32768.0f;
                    }
                    mono_buf[i] = static_cast<int16_t>(
                        std::max(-32768.0f, std::min(32767.0f, sample * 32767.0f)));
                }
                dest_.write(mono_buf.data(), frames);
            }

            capture->ReleaseBuffer(frames);
            capture->GetNextPacketSize(&packet_size);
        }
        Sleep(5);
    }

    client->Stop();
    cleanup();
    LOG_INFO("WASAPI capture stopped");
}
