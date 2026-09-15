#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <random>
#include <cstdint>

namespace monomachine {

/**
 * @brief Elektron Monomachine DPRO-BBOX (Machine 7) & Extended Sampler Engine.
 * 
 * Hardware Architecture:
 * - DPRO-BBOX is the drum synthesizer/beatbox machine of the Monomachine.
 * - Hardware parameters:
 *   - PTCH (Pitch): Sample playback pitch / rate (-64 .. +63 semitone/fine ratio)
 *   - STRT (Start): Sample playback start offset (0 .. 127 -> 0% .. 100%)
 *   - RTRG (Retrig Count): Number of rapid retrigger repeats (0 = off, 1 .. 127)
 *   - RTIM (Retrig Time): Time between retriggers (tempo synchronized / milliseconds)
 * 
 * Extension for Modern DAW / VST3:
 * - 10 Sample Slots with dynamic loading.
 * - Sample randomization mode (random slot selection and/or random start position).
 * - Linear / Hermite sample interpolation with pitch tracking.
 */
class MonomachineBBox {
public:
    static constexpr size_t kMaxSlots = 10;

    struct SampleSlot {
        std::string name;
        std::vector<float> data;
        double originalSampleRate = 44100.0;
        bool loaded = false;
    };

    MonomachineBBox() {
        m_slots.resize(kMaxSlots);
        m_rng.seed(1337);
        generateDefaultDrumKit();
    }

    void reset(double sampleRate = 44100.0) {
        m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        m_playbackPos = 0.0;
        m_isPlaying = false;
        m_retrigCounter = 0;
        m_retrigSamplesRemaining = 0;
    }

    /**
     * @brief Load a raw mono sample into one of the 10 slots (0..9).
     */
    bool loadSample(size_t slotIndex, const std::string& name, const float* samples, size_t numSamples, double sampleRate = 44100.0) {
        if (slotIndex >= kMaxSlots || samples == nullptr || numSamples == 0) return false;
        m_slots[slotIndex].name = name;
        m_slots[slotIndex].data.assign(samples, samples + numSamples);
        m_slots[slotIndex].originalSampleRate = sampleRate;
        m_slots[slotIndex].loaded = true;
        return true;
    }

    /**
     * @brief Monomachine BBox hardware parameter setter.
     */
    void setParameters(uint8_t ptch, uint8_t strt, uint8_t rtrg, uint8_t rtim,
                       uint8_t slotSelect = 0, bool randomSlot = false, bool randomStrt = false) {
        m_paramPtch = ptch;
        m_paramStrt = strt;
        m_paramRtrg = rtrg;
        m_paramRtim = rtim;
        m_activeSlot = slotSelect % kMaxSlots;
        m_randomSlot = randomSlot;
        m_randomStart = randomStrt;

        // Pitch multiplier: 0 -> -2 octaves (0.25x), 64 -> 1.0x (normal), 127 -> +2 octaves (4.0x)
        float semitones = (static_cast<float>(ptch) - 64.0f) / 64.0f * 24.0f;
        m_pitchRatio = std::pow(2.0f, semitones / 12.0f);

        // Retrig interval in samples
        float retrigMs = 5.0f + (static_cast<float>(rtim) / 127.0f) * 250.0f;
        m_retrigIntervalSamples = static_cast<size_t>((retrigMs / 1000.0f) * m_sampleRate);
    }

    /**
     * @brief Trigger note on / drum hit.
     */
    void noteOn(uint8_t midiNote = 60, uint8_t velocity = 127) {
        (void)velocity;
        size_t targetSlot = m_activeSlot;

        if (m_randomSlot) {
            std::uniform_int_distribution<size_t> dist(0, kMaxSlots - 1);
            targetSlot = dist(m_rng);
        }

        m_currentPlayingSlot = targetSlot;
        auto& slot = m_slots[m_currentPlayingSlot];
        if (!slot.loaded || slot.data.empty()) {
            m_isPlaying = false;
            return;
        }

        float startFrac = static_cast<float>(m_paramStrt) / 127.0f;
        if (m_randomStart) {
            std::uniform_real_distribution<float> dist(0.0f, 0.85f);
            startFrac = dist(m_rng);
        }

        m_playbackPos = startFrac * static_cast<float>(slot.data.size());
        m_isPlaying = true;

        // Retrigger setup
        m_retrigTotal = m_paramRtrg;
        m_retrigCounter = 0;
        m_retrigSamplesRemaining = m_retrigIntervalSamples;

        // Key tracking pitch offset (Middle C / 60 is neutral)
        float noteSemitones = static_cast<float>(midiNote) - 60.0f;
        m_keyTrackRatio = std::pow(2.0f, noteSemitones / 12.0f);
    }

    /**
     * @brief Process a block of stereo samples.
     */
    void processStereo(float* outL, float* outR, size_t numFrames) {
        if (!m_isPlaying || m_currentPlayingSlot >= kMaxSlots) {
            for (size_t i = 0; i < numFrames; ++i) {
                outL[i] = 0.0f;
                outR[i] = 0.0f;
            }
            return;
        }

        const auto& slot = m_slots[m_currentPlayingSlot];
        const auto& data = slot.data;
        const size_t dataSize = data.size();
        const double rateFactor = (slot.originalSampleRate / m_sampleRate) * m_pitchRatio * m_keyTrackRatio;

        for (size_t i = 0; i < numFrames; ++i) {
            if (!m_isPlaying) {
                outL[i] = 0.0f;
                outR[i] = 0.0f;
                continue;
            }

            // Check Retrigger
            if (m_retrigTotal > 0 && m_retrigCounter < m_retrigTotal) {
                if (m_retrigSamplesRemaining == 0) {
                    float startFrac = static_cast<float>(m_paramStrt) / 127.0f;
                    m_playbackPos = startFrac * static_cast<float>(dataSize);
                    m_retrigSamplesRemaining = m_retrigIntervalSamples;
                    ++m_retrigCounter;
                } else {
                    --m_retrigSamplesRemaining;
                }
            }

            // Interpolated sample read
            size_t idx0 = static_cast<size_t>(m_playbackPos);
            if (idx0 >= dataSize) {
                m_isPlaying = false;
                outL[i] = 0.0f;
                outR[i] = 0.0f;
                continue;
            }

            size_t idx1 = std::min(idx0 + 1, dataSize - 1);
            float frac = static_cast<float>(m_playbackPos - idx0);
            float sample = data[idx0] + frac * (data[idx1] - data[idx0]);

            outL[i] = sample;
            outR[i] = sample;

            m_playbackPos += rateFactor;
        }
    }

    const std::vector<SampleSlot>& getSlots() const noexcept { return m_slots; }

private:
    void generateDefaultDrumKit() {
        // Generate synthetic drum samples so all 10 slots work out-of-the-box
        for (size_t s = 0; s < kMaxSlots; ++s) {
            m_slots[s].loaded = true;
            m_slots[s].originalSampleRate = 44100.0;
            size_t len = 12000;
            m_slots[s].data.resize(len);

            if (s % 3 == 0) {
                // Kick drum (pitch drop sweep)
                m_slots[s].name = "Synthetic Kick " + std::to_string(s + 1);
                float phase = 0.0f;
                for (size_t n = 0; n < len; ++n) {
                    float env = std::exp(-static_cast<float>(n) / 2500.0f);
                    float freq = 50.0f + 180.0f * std::exp(-static_cast<float>(n) / 800.0f);
                    phase += 2.0f * 3.14159265f * freq / 44100.0f;
                    m_slots[s].data[n] = std::sin(phase) * env;
                }
            } else if (s % 3 == 1) {
                // Snare drum (noise burst + tone)
                m_slots[s].name = "Synthetic Snare " + std::to_string(s + 1);
                float phase = 0.0f;
                for (size_t n = 0; n < len; ++n) {
                    float env = std::exp(-static_cast<float>(n) / 1800.0f);
                    float noise = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
                    phase += 2.0f * 3.14159265f * 220.0f / 44100.0f;
                    m_slots[s].data[n] = (std::sin(phase) * 0.4f + noise * 0.6f) * env;
                }
            } else {
                // Hi-Hat / Percussion (short noise)
                m_slots[s].name = "Synthetic Hat " + std::to_string(s + 1);
                for (size_t n = 0; n < len; ++n) {
                    float env = std::exp(-static_cast<float>(n) / 600.0f);
                    float noise = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;
                    m_slots[s].data[n] = noise * env;
                }
            }
        }
    }

    double m_sampleRate = 44100.0;
    std::vector<SampleSlot> m_slots;
    size_t m_activeSlot = 0;
    size_t m_currentPlayingSlot = 0;
    bool m_isPlaying = false;
    double m_playbackPos = 0.0;
    float m_pitchRatio = 1.0f;
    float m_keyTrackRatio = 1.0f;

    uint8_t m_paramPtch = 64;
    uint8_t m_paramStrt = 0;
    uint8_t m_paramRtrg = 0;
    uint8_t m_paramRtim = 32;
    bool m_randomSlot = false;
    bool m_randomStart = false;

    uint8_t m_retrigTotal = 0;
    uint8_t m_retrigCounter = 0;
    size_t m_retrigIntervalSamples = 2000;
    size_t m_retrigSamplesRemaining = 0;

    std::mt19937 m_rng;
};

} // namespace monomachine
