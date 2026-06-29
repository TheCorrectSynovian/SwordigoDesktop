/* av_audio.h — Audio playback module for the Swordigo Asset Viewer
 *
 * Provides WAV file playback using SDL3's built-in audio API.
 * Features:
 *   - Load WAV files (any format SDL3 supports)
 *   - Play / Pause / Stop / Seek
 *   - Volume control (0.0 – 1.0)
 *   - Pre-computed waveform data for visualization (~500 samples)
 *
 * All audio is converted to float32 internally for waveform generation.
 * Playback uses SDL_OpenAudioDeviceStream + SDL_PutAudioStreamData.
 */

#ifndef AV_AUDIO_H
#define AV_AUDIO_H

#include <string>
#include <vector>

namespace av {

// ----------------------------------------------------------------------------
// AudioState — current state of the audio player, updated each frame
// ----------------------------------------------------------------------------

struct AudioState {
    bool loaded  = false;
    bool playing = false;
    bool paused  = false;

    float duration = 0.0f;   // total duration in seconds
    float position = 0.0f;   // current playback position in seconds

    float volume = 0.8f;     // 0.0 (silent) to 1.0 (full)

    int sample_rate    = 0;
    int channels       = 0;
    int bits_per_sample = 0;

    // Normalized waveform samples for visualization (~500 points).
    // Values in [-1.0, 1.0], downsampled from the full PCM data.
    // Stereo is mixed to mono before downsampling.
    std::vector<float> waveform;

    std::string filename;    // basename of the loaded file
};

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

/// Initialize the audio subsystem. Call once at startup.
/// Returns true on success.
bool audio_init();

/// Shut down the audio subsystem. Call once at exit.
/// Frees all resources (stream, WAV buffer, etc.).
void audio_shutdown();

/// Load a WAV file from `path`. Replaces any previously loaded audio.
/// Returns true on success. On failure, state.loaded remains false.
bool audio_load(const std::string& path);

/// Begin or resume playback of the loaded audio.
void audio_play();

/// Pause playback (can be resumed with audio_play).
void audio_pause();

/// Stop playback and reset position to 0.
void audio_stop();

/// Seek to a specific position in seconds.
/// Clamps to [0, duration]. Takes effect immediately.
void audio_seek(float seconds);

/// Set playback volume. Clamped to [0.0, 1.0].
void audio_set_volume(float vol);

/// Get a reference to the current audio state.
/// The state is updated each frame by audio_update().
AudioState& audio_get_state();

/// Call once per frame to update the playback position.
/// This tracks how many samples have been consumed by the device.
void audio_update();

} // namespace av

#endif // AV_AUDIO_H
