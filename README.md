# LAN Audio Bridge & Mixer

A real-time C++ application that streams audio from your Android phone over LAN,
captures your laptop's system audio via WASAPI loopback, mixes both streams, and
outputs the result to your headphones — all with sub-100ms latency.

---

## What It Does

```
[Android Phone] ──TCP/LAN──▶ ┐
                              ├──▶ Mixer ──▶ Headphones
[Laptop System Audio] ────────┘
```

- Receives raw PCM audio from your phone over a TCP socket
- Captures laptop system audio using WASAPI loopback (Windows only)
- Mixes both streams with adjustable per-source volume
- Outputs the final mix to your default audio output device via PortAudio
- Runtime volume control via keyboard commands

---

## Project Structure

```
main-setup/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp                      ← entry point, wires all modules
│   ├── network/
│   │   ├── packet.hpp                ← wire protocol (header format)
│   │   ├── tcp_server.hpp
│   │   └── tcp_server.cpp            ← receives phone audio over TCP
│   ├── audio/
│   │   ├── ring_buffer.hpp           ← lock-free SPSC circular buffer
│   │   ├── wasapi_capture.hpp
│   │   ├── wasapi_capture.cpp        ← WASAPI loopback system audio capture
│   │   ├── portaudio_output.hpp
│   │   └── portaudio_output.cpp      ← PortAudio playback to headphones
│   ├── mixer/
│   │   ├── mixer.hpp
│   │   └── mixer.cpp                 ← combines phone + system audio
│   └── utils/
│       └── logger.hpp                ← simple timestamped console logger
├── third_party/
│   ├── asio/                         ← (optional) standalone ASIO for UDP phase
│   └── portaudio/                    ← place your PortAudio build here
└── android/
    └── AudioSender/                  ← Android sender app (Phase 4)
```

---

## Dependencies

| Library    | Purpose                        | How to get |
|------------|--------------------------------|------------|
| PortAudio  | Cross-platform audio I/O       | Build from source: http://www.portaudio.com/download.html |
| WASAPI     | Windows system audio capture   | Built into Windows SDK — nothing to install |
| Winsock2   | TCP networking                 | Built into Windows SDK — nothing to install |
| CMake 3.20+| Build system                   | https://cmake.org/download/ |

---

## Building

### 1. Build PortAudio

```bash
git clone https://github.com/PortAudio/portaudio.git
cd portaudio
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

This produces `portaudio.lib` (MSVC) or `libportaudio.a` (MinGW).

### 2. Build this project

```bash
cd main-setup
mkdir build && cd build

# Point PORTAUDIO_ROOT at your PortAudio build/install directory
cmake .. -DPORTAUDIO_ROOT=C:/path/to/portaudio/build

cmake --build . --config Release
```

The output binary is `lan_audio_bridge.exe` inside the build folder.

---

## Running

```bash
./lan_audio_bridge.exe
```

On startup it:
1. Opens a TCP server on port **9000**
2. Starts WASAPI loopback capture
3. Starts the mixer
4. Opens PortAudio output to your default device

Connect your phone (see Android section below) and audio will start flowing.

### Runtime Commands

While running, type commands in the terminal:

| Command   | Effect                          |
|-----------|---------------------------------|
| `p 0.8`   | Set phone volume to 0.8         |
| `s 1.2`   | Set system audio volume to 1.2  |
| `q`       | Quit cleanly                    |

Volume range: `0.0` (mute) to `2.0` (double gain). Default is `1.0` for both.

---

## Network Protocol

Every audio chunk sent from the phone is prefixed with a fixed-size header:

```
┌──────────────┬──────────────────┬──────────────────┐
│  magic (4B)  │ payload_size (4B)│ timestamp_ms (8B)│
└──────────────┴──────────────────┴──────────────────┘
│         PCM int16 mono samples (payload_size bytes) │
└─────────────────────────────────────────────────────┘
```

- `magic` = `0xABCD1234` (sanity check, big-endian)
- `payload_size` = number of bytes of PCM data that follow
- `timestamp_ms` = sender-side timestamp (used later for jitter stats)
- PCM format: **16-bit signed, mono, 44100 Hz**

---

## Audio Format

| Parameter   | Value         |
|-------------|---------------|
| Encoding    | PCM 16-bit signed |
| Channels    | Mono (stereo in a later phase) |
| Sample rate | 44100 Hz      |
| Chunk size  | 512 samples (~11.6 ms) |

---

## Architecture Deep Dive

### Ring Buffer (`ring_buffer.hpp`)

Lock-free single-producer single-consumer (SPSC) circular buffer.
Used between every producer/consumer pair:
- Network thread → phone RingBuffer
- WASAPI thread → system RingBuffer
- Mixer thread reads both, writes → output RingBuffer
- PortAudio callback reads output RingBuffer

Capacity is set to 2 seconds of audio (88200 samples) to absorb jitter.
On overflow, new samples are dropped. On underrun, silence is inserted.

### TCP Server (`tcp_server.cpp`)

- Runs on its own thread
- Accepts one client at a time (phone)
- Reads header → validates magic → reads payload → pushes to ring buffer
- Reconnects automatically if the phone disconnects

### WASAPI Capture (`wasapi_capture.cpp`)

- Uses WASAPI loopback mode (`AUDCLNT_STREAMFLAGS_LOOPBACK`)
- Captures whatever is playing on the default render device
- Converts float32 or int16 multi-channel to int16 mono
- Polls every 5ms for new packets

### Mixer (`mixer.cpp`)

- Runs on its own thread, wakes every 2ms
- Reads 512 samples from each source (zero-pads if underrun)
- Applies per-source float volume multiplier
- Sums and hard-clips to int16 range [-32768, 32767]
- Writes result to output ring buffer

### PortAudio Output (`portaudio_output.cpp`)

- Opens default output device, mono, 44100 Hz, int16
- PortAudio calls the callback every ~11.6ms (512 frames)
- Callback reads from output ring buffer, zero-fills on underrun

---

## Development Roadmap

### ✅ Phase 1 — MVP (done)
- TCP server receives raw PCM from phone
- PortAudio plays it directly
- Ring buffer smooths playback

### ✅ Phase 2 — System audio capture (done)
- WASAPI loopback captures laptop audio

### ✅ Phase 3 — Mixer (done)
- Both streams mixed with volume control

### 🔲 Phase 4 — Android sender app
- Kotlin app using `AudioRecord` (mic) or `AudioPlaybackCapture` (internal audio)
- Sends PCM chunks with the header format above over TCP

### 🔲 Phase 5 — UDP + latency optimization
- Switch network layer to UDP for lower latency
- Add jitter buffer with adaptive sizing
- Tune chunk size (try 256 samples)

### 🔲 Phase 6 — Optional upgrades
- GUI with volume sliders
- Auto device discovery (LAN scan / mDNS)
- Latency monitor display
- Multi-device support
- Stereo output

---

## Android Sender (Phase 4 Preview)

The Android app needs to:

1. Capture mic audio with `AudioRecord` at 44100 Hz, mono, PCM 16-bit
2. For each chunk of 512 samples (1024 bytes):
   - Build the header: magic=`0xABCD1234`, payload_size=1024, timestamp=current ms
   - Send header bytes then payload bytes over a TCP socket to laptop IP:9000
3. Handle reconnection on socket error

Kotlin snippet (core loop):

```kotlin
val socket = Socket("192.168.x.x", 9000)
val out = DataOutputStream(socket.getOutputStream())
val buf = ShortArray(512)
val audioRecord = AudioRecord(
    MediaRecorder.AudioSource.MIC,
    44100, AudioFormat.CHANNEL_IN_MONO,
    AudioFormat.ENCODING_PCM_16BIT, 1024
)
audioRecord.startRecording()

while (running) {
    audioRecord.read(buf, 0, 512)
    val bytes = ByteBuffer.allocate(16 + 1024)
        .order(ByteOrder.BIG_ENDIAN)
        .putInt(0xABCD1234.toInt())
        .putInt(1024)
        .putLong(System.currentTimeMillis())
    buf.forEach { bytes.putShort(it) }
    out.write(bytes.array())
}
```

---

## Known Challenges

| Challenge | Mitigation |
|-----------|------------|
| WASAPI format varies per device | Runtime format detection + float→int16 conversion |
| Network jitter | 2-second ring buffer absorbs bursts |
| Audio clipping when mixing | Hard clip at int16 bounds (soft limiter in Phase 5) |
| Phone/laptop sample rate mismatch | Both locked to 44100 Hz for now |
| Latency | 512-sample chunks = ~11.6ms per hop; total ~50-80ms on LAN |

---

## License

MIT — do whatever you want with it.
