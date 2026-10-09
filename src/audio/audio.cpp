// Port of audio.js
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_GENERATION
#include "audio/audio.h"
#include <miniaudio.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <vector>
#include "core/math.h"

namespace {

enum class Wave { Sine, Triangle, Square, Sawtooth };

// one oscillator note with the envelope tone() builds in WebAudio:
// 0 -> vol over 10 ms, exponential decay to 0.001 at `dur`, frequency sliding exponentially by `slide`
struct Voice {
    Wave wave;
    double freq, slide, start, dur, vol, phase = 0;
};

// noise burst through a band-pass filter (crunch() for footsteps)
struct Crunch {
    double pos = 0, rate;
    float b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};

}  // namespace

struct Audio::Impl {
    ma_device device{};
    ma_decoder music{};
    bool deviceOk = false, musicOk = false, started = false;
    std::atomic<bool> muted{false};
    std::mutex lock;
    std::vector<Voice> voices;
    std::vector<Crunch> crunches;
    std::vector<float> noise;
    double now = 0;
    const double sr = 48000;

    static void callback(ma_device* dev, void* out, const void*, ma_uint32 frames) {
        auto* self = (Impl*)dev->pUserData;
        self->mix((float*)out, frames);
    }

    void mix(float* out, ma_uint32 frames) {
        std::fill(out, out + frames * 2, 0.0f);
        if (musicOk && !muted) {
            std::vector<float> buf(frames * 2);
            ma_uint64 read = 0;
            ma_decoder_read_pcm_frames(&music, buf.data(), frames, &read);
            if (read < frames) {
                // loop
                ma_decoder_seek_to_pcm_frame(&music, 0);
                ma_uint64 more = 0;
                ma_decoder_read_pcm_frames(&music, buf.data() + read * 2, frames - read, &more);
            }
            for (ma_uint32 i = 0; i < frames * 2; ++i) out[i] += buf[i] * 0.45f;
        }
        std::lock_guard<std::mutex> g(lock);
        for (ma_uint32 i = 0; i < frames; ++i) {
            double t = now + i / sr;
            float s = 0;
            for (auto& v : voices) {
                if (t < v.start || t > v.start + v.dur + 0.05) continue;
                double lt = t - v.start;
                double f = v.slide ? v.freq * std::pow(v.slide, std::min(1.0, lt / v.dur)) : v.freq;
                v.phase += f / sr;
                double ph = v.phase - std::floor(v.phase);
                double w;
                switch (v.wave) {
                    case Wave::Sine: w = std::sin(ph * 2 * PI); break;
                    case Wave::Triangle: w = 1 - 4 * std::abs(ph - 0.5); break;
                    case Wave::Square: w = ph < 0.5 ? 1 : -1; break;
                    default: w = 2 * ph - 1; break;
                }
                double env;
                if (lt < 0.01) env = v.vol * lt / 0.01;
                else if (lt < v.dur) env = v.vol * std::pow(0.001 / v.vol, (lt - 0.01) / std::max(1e-3, v.dur - 0.01));
                else env = 0.001;
                s += (float)(w * env);
            }
            for (auto& c : crunches) {
                size_t k = (size_t)c.pos;
                if (k >= noise.size()) continue;
                float x = noise[k];
                c.pos += c.rate;
                float y = c.b0 * x + c.b1 * c.x1 + c.b2 * c.x2 - c.a1 * c.y1 - c.a2 * c.y2;
                c.x2 = c.x1;
                c.x1 = x;
                c.y2 = c.y1;
                c.y1 = y;
                s += y * 0.05f;
            }
            out[i * 2] += s;
            out[i * 2 + 1] += s;
        }
        now += frames / sr;
        voices.erase(std::remove_if(voices.begin(), voices.end(), [&](const Voice& v) { return now > v.start + v.dur + 0.06; }), voices.end());
        crunches.erase(std::remove_if(crunches.begin(), crunches.end(), [&](const Crunch& c) { return c.pos >= noise.size(); }), crunches.end());
    }

    void tone(double freq, double start, double dur, Wave wave = Wave::Sine, double vol = 0.15, double slide = 0) {
        if (muted || !deviceOk) return;
        std::lock_guard<std::mutex> g(lock);
        voices.push_back({wave, freq, slide, now + 0.02 + start, dur, vol});
    }

    void crunch() {
        if (muted || !deviceOk) return;
        Crunch c;
        c.rate = 0.8 + frand() * 0.5;
        // RBJ band-pass (constant 0 dB peak), Q = 1 like BiquadFilterNode's default
        double f0 = 1400 + frand() * 600, w0 = 2 * PI * f0 / sr, alpha = std::sin(w0) / 2;
        double a0 = 1 + alpha;
        c.b0 = (float)(alpha / a0);
        c.b1 = 0;
        c.b2 = (float)(-alpha / a0);
        c.a1 = (float)(-2 * std::cos(w0) / a0);
        c.a2 = (float)((1 - alpha) / a0);
        std::lock_guard<std::mutex> g(lock);
        crunches.push_back(c);
    }
};

Audio::Audio() : d(std::make_unique<Impl>()) {
    // 80 ms of decaying white noise
    size_t n = (size_t)(d->sr * 0.08);
    d->noise.resize(n);
    for (size_t i = 0; i < n; ++i) d->noise[i] = (frand() * 2 - 1) * std::pow(1 - (float)i / n, 2.0f);
}

Audio::~Audio() {
    if (d->deviceOk) ma_device_uninit(&d->device);
    if (d->musicOk) ma_decoder_uninit(&d->music);
}

void Audio::unlock() {
    if (d->started) return;
    d->started = true;
    std::string path = std::string(ASSET_DIR) + "/audio/xmas.mp3";
    ma_decoder_config dc = ma_decoder_config_init(ma_format_f32, 2, (ma_uint32)d->sr);
    d->musicOk = ma_decoder_init_file(path.c_str(), &dc, &d->music) == MA_SUCCESS;
    if (!d->musicOk) std::fprintf(stderr, "audio: cannot open %s\n", path.c_str());
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = (ma_uint32)d->sr;
    cfg.dataCallback = Impl::callback;
    cfg.pUserData = d.get();
    d->deviceOk = ma_device_init(nullptr, &cfg, &d->device) == MA_SUCCESS && ma_device_start(&d->device) == MA_SUCCESS;
    if (!d->deviceOk) std::fprintf(stderr, "audio: no output device\n");
}

bool Audio::toggleMute() {
    d->muted = !d->muted;
    return d->muted;
}

bool Audio::muted() const { return d->muted; }

void Audio::step() { d->crunch(); }
void Audio::jump() { d->tone(420, 0, 0.18, Wave::Triangle, 0.08, 1.8); }
void Audio::land() {
    d->crunch();
    d->crunch();
}
void Audio::chime() {
    // bell notes C E G C
    const double f[4] = {523, 659, 784, 1047};
    for (int i = 0; i < 4; ++i) {
        d->tone(f[i], i * 0.09, 0.6, Wave::Sine, 0.12);
        d->tone(f[i] * 2, i * 0.09, 0.3, Wave::Triangle, 0.03);
    }
}
void Audio::pop() {
    d->tone(300, 0, 0.12, Wave::Square, 0.06, 3);
    d->tone(900, 0.1, 0.25, Wave::Sine, 0.08);
}
void Audio::talk() { d->tone(600 + frand() * 200, 0, 0.05, Wave::Square, 0.025); }
void Audio::fanfare() {
    const double f[7] = {523, 659, 784, 1047, 784, 1047, 1319};
    for (int i = 0; i < 7; ++i) d->tone(f[i], i * 0.13, 0.5, Wave::Triangle, 0.1);
}
void Audio::boom() { d->tone(90, 0, 0.6, Wave::Sawtooth, 0.05, 0.4); }
