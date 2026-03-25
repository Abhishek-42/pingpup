#include <thread>
#include <chrono>
#include <string>
#include <atomic>

#include "ui/gui.hpp"
#include "audio/ring_buffer.hpp"
#include "audio/wasapi_capture.hpp"
#include "audio/portaudio_output.hpp"
#include "network/udp_receiver.hpp"
#include "mixer/mixer.hpp"
#include "utils/logger.hpp"

// Redirect LOG macros into AppState so messages appear in the GUI log panel
#undef  LOG_INFO
#undef  LOG_WARN
#undef  LOG_ERR
#define LOG_INFO(msg) do { g_state->push_log("[INFO] " + std::string(msg)); } while(0)
#define LOG_WARN(msg) do { g_state->push_log("[WARN] " + std::string(msg)); } while(0)
#define LOG_ERR(msg)  do { g_state->push_log("[ERR]  " + std::string(msg)); } while(0)

static AppState* g_state = nullptr;

static constexpr int    SAMPLE_RATE  = 44100;
static constexpr int    CHUNK        = 256;
static constexpr size_t BUF_CAPACITY = SAMPLE_RATE * 2; // 2 seconds

static int buf_pct(const RingBuffer& b) {
    return static_cast<int>(b.available() * 100 / BUF_CAPACITY);
}

int main() {
    AppState state;
    g_state = &state;

    // ── Ring buffers ──────────────────────────────────────────────────────
    RingBuffer phone_buf(BUF_CAPACITY);
    RingBuffer system_buf(BUF_CAPACITY);
    RingBuffer output_buf(BUF_CAPACITY);

    // ── Audio engine ──────────────────────────────────────────────────────
    UdpReceiver     udp(9000, phone_buf);
    WasapiCapture   wasapi(system_buf);
    Mixer           mixer(phone_buf, system_buf, output_buf, CHUNK);
    PortAudioOutput pa_out(output_buf, SAMPLE_RATE, CHUNK);

    udp.start();

    if (!wasapi.start())
        state.push_log("[WARN] WASAPI failed — system audio silent");
    else
        state.system_active = true;

    mixer.start();

    if (!pa_out.start()) {
        state.push_log("[ERR]  PortAudio failed — cannot output audio");
        state.running = false;
    }

    state.push_log("[INFO] Listening on UDP port 9000");
    state.push_log("[INFO] Send audio from phone to this PC");

    // ── Stats thread — updates GUI state every 100ms ──────────────────────
    std::thread stats_thread([&]() {
        auto t0 = std::chrono::steady_clock::now();
        while (state.running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            state.phone_active    = udp.client_active();
            state.phone_buf_pct   = buf_pct(phone_buf);
            state.system_buf_pct  = buf_pct(system_buf);
            state.output_buf_pct  = buf_pct(output_buf);

            // Rough latency estimate: chunk duration * 3 hops + network ~5ms
            float chunk_ms = (CHUNK * 1000.0f) / SAMPLE_RATE;
            state.latency_ms = chunk_ms * 3.0f + 5.0f;

            // Sync volumes from GUI → mixer
            mixer.set_phone_volume(state.phone_vol.load());
            mixer.set_system_volume(state.system_vol.load());
        }
    });

    // ── GUI runs on main thread (required by Win32) ───────────────────────
    run_gui(state);

    // ── Shutdown ──────────────────────────────────────────────────────────
    state.running = false;
    if (stats_thread.joinable()) stats_thread.join();

    pa_out.stop();
    mixer.stop();
    wasapi.stop();
    udp.stop();

    return 0;
}
