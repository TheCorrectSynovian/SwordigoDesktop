/* av_audio.cpp — Audio playback implementation for the Swordigo Asset Viewer
 *
 * Uses SDL3's audio stream API for playback:
 *   - SDL_LoadWAV to load the raw PCM data
 *   - SDL_ConvertAudioSamples to get float32 for waveform generation
 *   - SDL_OpenAudioDeviceStream for playback (manages device lifetime)
 *   - SDL_PutAudioStreamData to feed PCM chunks
 *
 * Playback position is tracked by counting how many bytes we've fed to the
 * stream minus what's still queued (not yet consumed by the device).
 *
 * The waveform is pre-computed on load: the full float32 PCM buffer is
 * downsampled to ~500 points (peak values per window) for ImGui rendering.
 */

#include "av_audio.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace av {

// ============================================================================
// Internal state
// ============================================================================

// The raw WAV data as loaded by SDL_LoadWAV (original format).
static Uint8*    s_wav_buf    = nullptr;
static Uint32    s_wav_len    = 0;
static SDL_AudioSpec s_wav_spec = {};  // format of the loaded WAV

// Float32 copy of the audio data (for waveform + seeking accuracy).
// We convert once on load and keep it around.
static Uint8*    s_f32_buf    = nullptr;
static int       s_f32_len    = 0;   // length in bytes

// Playback stream (owns the audio device).
static SDL_AudioStream* s_stream = nullptr;

// Playback tracking
static int  s_playback_cursor = 0;   // byte offset into s_wav_buf we've fed so far
static int  s_total_fed       = 0;   // total bytes pushed to stream (since play/seek)

// How many bytes we feed per update tick.  We feed in small chunks so we can
// track position accurately without starving the device.
// ~4096 samples * 4 bytes * 2 channels = 32768 bytes ≈ 0.04s at 48kHz stereo.
static const int FEED_CHUNK_BYTES = 32768;

// Shared state returned to callers.
static AudioState s_state;

// ============================================================================
// Helpers
// ============================================================================

/// Bytes per sample-frame (all channels) for the loaded WAV.
static int bytes_per_frame() {
    // SDL3 SDL_AudioSpec: format contains bit-size info; use SDL helper.
    return SDL_AUDIO_BYTESIZE(s_wav_spec.format) * s_wav_spec.channels;
}

/// Total number of sample-frames in the WAV.
static int total_frames() {
    int bpf = bytes_per_frame();
    return (bpf > 0) ? (int)(s_wav_len / (Uint32)bpf) : 0;
}

/// Convert a byte offset (in the WAV buffer) to seconds.
static float bytes_to_seconds(int byte_offset) {
    int bpf = bytes_per_frame();
    if (bpf <= 0 || s_wav_spec.freq <= 0) return 0.0f;
    int frames = byte_offset / bpf;
    return (float)frames / (float)s_wav_spec.freq;
}

/// Convert seconds to a byte offset (frame-aligned) in the WAV buffer.
static int seconds_to_bytes(float seconds) {
    int bpf = bytes_per_frame();
    if (bpf <= 0 || s_wav_spec.freq <= 0) return 0;
    int frame = (int)(seconds * (float)s_wav_spec.freq);
    frame = std::clamp(frame, 0, total_frames());
    return frame * bpf;
}

/// Build the waveform visualization data from the float32 buffer.
/// Downsamples to `target_points` by taking peak absolute values per window.
/// Stereo is mixed to mono (average of L and R).
static void build_waveform(int target_points) {
    s_state.waveform.clear();
    if (!s_f32_buf || s_f32_len <= 0) return;

    // The float32 buffer uses s_wav_spec.channels channels.
    int channels    = s_wav_spec.channels;
    int total_samples = s_f32_len / (int)sizeof(float);  // total float values
    int total_mono    = total_samples / std::max(channels, 1);  // mono frames

    if (total_mono <= 0) return;

    target_points = std::clamp(target_points, 10, 2000);
    s_state.waveform.resize(target_points);

    float* fdata = reinterpret_cast<float*>(s_f32_buf);

    for (int i = 0; i < target_points; ++i) {
        // Window of mono frames that maps to this waveform point
        int win_start = (int)((int64_t)i * total_mono / target_points);
        int win_end   = (int)((int64_t)(i + 1) * total_mono / target_points);
        win_end = std::min(win_end, total_mono);

        float peak = 0.0f;
        for (int f = win_start; f < win_end; ++f) {
            // Mix channels to mono by averaging
            float mono = 0.0f;
            for (int c = 0; c < channels; ++c) {
                mono += fdata[f * channels + c];
            }
            mono /= (float)channels;
            float a = std::fabs(mono);
            if (a > peak) peak = a;
        }
        s_state.waveform[i] = peak;
    }

    // Normalize so the largest peak is 1.0
    float max_peak = *std::max_element(s_state.waveform.begin(),
                                        s_state.waveform.end());
    if (max_peak > 0.0f) {
        for (float& v : s_state.waveform) {
            v /= max_peak;
        }
    }
}

/// Destroy the playback stream (and its associated audio device).
static void destroy_stream() {
    if (s_stream) {
        SDL_DestroyAudioStream(s_stream);
        s_stream = nullptr;
    }
}

/// Create a fresh playback stream for the current WAV spec.
/// Returns true on success.
static bool create_stream() {
    destroy_stream();

    // Open a playback stream on the default output device.
    // SDL_OpenAudioDeviceStream binds the stream to the device.
    // The device starts paused — we resume it when play() is called.
    s_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &s_wav_spec,   // source spec (our WAV format)
        nullptr,       // no callback — we push data manually
        nullptr        // no userdata
    );

    if (!s_stream) {
        SDL_Log("av_audio: failed to open audio device stream: %s",
                SDL_GetError());
        return false;
    }

    // Apply the current volume setting
    SDL_SetAudioStreamGain(s_stream, s_state.volume);

    return true;
}

// ============================================================================
// Public API
// ============================================================================

bool audio_init() {
    // SDL_Init(SDL_INIT_AUDIO) should already be called by the main app.
    // We just reset our state.
    s_state = AudioState{};
    return true;
}

void audio_shutdown() {
    audio_stop();
    destroy_stream();

    // Free the WAV buffer
    if (s_wav_buf) {
        SDL_free(s_wav_buf);
        s_wav_buf = nullptr;
    }
    s_wav_len = 0;

    // Free the float32 buffer
    if (s_f32_buf) {
        SDL_free(s_f32_buf);
        s_f32_buf = nullptr;
    }
    s_f32_len = 0;

    s_state = AudioState{};
}

bool audio_load(const std::string& path) {
    // Stop any current playback
    audio_stop();
    destroy_stream();

    // Free previous buffers
    if (s_wav_buf) {
        SDL_free(s_wav_buf);
        s_wav_buf = nullptr;
    }
    if (s_f32_buf) {
        SDL_free(s_f32_buf);
        s_f32_buf = nullptr;
    }
    s_wav_len = 0;
    s_f32_len = 0;
    s_state = AudioState{};

    // ---- Load the WAV file ----
    if (!SDL_LoadWAV(path.c_str(), &s_wav_spec, &s_wav_buf, &s_wav_len)) {
        SDL_Log("av_audio: failed to load WAV '%s': %s",
                path.c_str(), SDL_GetError());
        return false;
    }

    // ---- Populate state metadata ----
    s_state.loaded         = true;
    s_state.sample_rate    = s_wav_spec.freq;
    s_state.channels       = s_wav_spec.channels;
    s_state.bits_per_sample = (int)SDL_AUDIO_BITSIZE(s_wav_spec.format);
    s_state.duration       = bytes_to_seconds((int)s_wav_len);
    s_state.position       = 0.0f;
    s_state.filename       = fs::path(path).filename().string();

    // ---- Convert to float32 for waveform generation ----
    SDL_AudioSpec f32_spec  = s_wav_spec;
    f32_spec.format         = SDL_AUDIO_F32;

    if (!SDL_ConvertAudioSamples(&s_wav_spec, s_wav_buf, (int)s_wav_len,
                                 &f32_spec, &s_f32_buf, &s_f32_len)) {
        SDL_Log("av_audio: float32 conversion failed: %s", SDL_GetError());
        // Non-fatal: we just won't have a waveform
    }

    // ---- Build waveform visualization ----
    build_waveform(500);

    // ---- Create the playback stream ----
    if (!create_stream()) {
        // Stream creation failed — we still have the data loaded,
        // just can't play it.  Leave state.loaded = true so the UI
        // can show metadata and waveform.
        SDL_Log("av_audio: playback stream creation failed");
    }

    s_playback_cursor = 0;
    s_total_fed       = 0;

    SDL_Log("av_audio: loaded '%s' — %.1fs, %dHz, %dch, %d-bit",
            s_state.filename.c_str(), s_state.duration,
            s_state.sample_rate, s_state.channels, s_state.bits_per_sample);

    return true;
}

void audio_play() {
    if (!s_state.loaded || !s_stream) return;

    if (s_state.paused) {
        // Resume from pause — just unpause the device
        SDL_ResumeAudioStreamDevice(s_stream);
        s_state.paused  = false;
        s_state.playing = true;
        return;
    }

    if (s_state.playing) return;  // already playing

    // Start fresh playback from the current cursor position
    SDL_ResumeAudioStreamDevice(s_stream);
    s_state.playing = true;
    s_state.paused  = false;
}

void audio_pause() {
    if (!s_state.loaded || !s_state.playing || !s_stream) return;

    SDL_PauseAudioStreamDevice(s_stream);
    s_state.playing = false;
    s_state.paused  = true;
}

void audio_stop() {
    if (!s_stream) {
        s_state.playing = false;
        s_state.paused  = false;
        s_state.position = 0.0f;
        return;
    }

    SDL_PauseAudioStreamDevice(s_stream);
    SDL_ClearAudioStream(s_stream);

    s_state.playing  = false;
    s_state.paused   = false;
    s_state.position = 0.0f;
    s_playback_cursor = 0;
    s_total_fed       = 0;
}

void audio_seek(float seconds) {
    if (!s_state.loaded) return;

    seconds = std::clamp(seconds, 0.0f, s_state.duration);

    // Compute the new cursor position in the WAV buffer
    s_playback_cursor = seconds_to_bytes(seconds);
    s_state.position  = seconds;

    if (s_stream) {
        // Clear any buffered data in the stream so we don't hear old audio
        SDL_ClearAudioStream(s_stream);
        s_total_fed = 0;
    }
}

void audio_set_volume(float vol) {
    s_state.volume = std::clamp(vol, 0.0f, 1.0f);
    if (s_stream) {
        SDL_SetAudioStreamGain(s_stream, s_state.volume);
    }
}

AudioState& audio_get_state() {
    return s_state;
}

void audio_update() {
    if (!s_state.loaded || !s_state.playing || !s_stream) return;

    // ---- Feed more data to the stream if it's getting low ----
    // We keep the stream fed with small chunks so position tracking stays
    // responsive.  SDL will buffer and consume data asynchronously.

    int queued = SDL_GetAudioStreamQueued(s_stream);

    // Keep at least ~0.1s of audio buffered (but not too much, or seeking
    // feels sluggish).  Feed up to FEED_CHUNK_BYTES per frame.
    int min_buffer = bytes_per_frame() * s_wav_spec.freq / 10;  // ~0.1s
    if (min_buffer < FEED_CHUNK_BYTES) min_buffer = FEED_CHUNK_BYTES;

    while (queued < min_buffer && s_playback_cursor < (int)s_wav_len) {
        int remaining = (int)s_wav_len - s_playback_cursor;
        int chunk     = std::min(remaining, FEED_CHUNK_BYTES);

        if (!SDL_PutAudioStreamData(s_stream,
                                     s_wav_buf + s_playback_cursor, chunk)) {
            SDL_Log("av_audio: PutAudioStreamData failed: %s",
                    SDL_GetError());
            break;
        }

        s_playback_cursor += chunk;
        s_total_fed       += chunk;
        queued            += chunk;
    }

    // ---- Update playback position ----
    // Position = (total bytes fed) - (bytes still queued) → bytes consumed
    queued = SDL_GetAudioStreamQueued(s_stream);
    int consumed = s_total_fed - queued;
    if (consumed < 0) consumed = 0;

    // The consumed count is relative to wherever we started feeding
    // (which accounts for seeking).  We need the absolute position:
    //   absolute_byte_pos = (cursor at seek start) + consumed
    // But since s_playback_cursor already advanced, we can compute:
    //   bytes_consumed_absolute = s_playback_cursor - (s_total_fed - consumed)
    //                           = s_playback_cursor - queued
    int abs_pos = s_playback_cursor - queued;
    if (abs_pos < 0) abs_pos = 0;

    s_state.position = bytes_to_seconds(abs_pos);

    // ---- Check if playback finished ----
    if (s_playback_cursor >= (int)s_wav_len && queued <= 0) {
        // We've fed everything and the device has consumed it all
        s_state.playing  = false;
        s_state.paused   = false;
        s_state.position = s_state.duration;
    }
}

} // namespace av
