#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace monomachine {

/**
 * @brief Elektron Monomachine FX-CHORUS (Machine 15) Native Implementation.
 * 
 * Hardware Architecture:
 * - Synthesis Page Parameters:
 *   - SYNTH_A (DEL): Base Delay Time (0..127 -> ~1ms to 30ms)
 *   - SYNTH_B (DEP): Modulation Depth (0..127)
 *   - SYNTH_C (SPD): LFO Modulation Speed (0..127 -> 0.05 Hz to 10 Hz)
 *   - SYNTH_D (MIX): Wet/Dry Balance (0: 100% Dry, 127: 100% Wet)
 *   - SYNTH_F (WID): Stereo Width Spread (Phase offset between L and R LFO)
 *   - SYNTH_H (INP): Input bus select (Track input / Neighbour routing)
 */
class MonomachineChorus {
public:
    MonomachineChorus() {
        m_bufferL.resize(kMaxDelaySamples, 0.0f);
        m_bufferR.resize(kMaxDelaySamples, 0.0f);
    }

    void reset(double sampleRate = 44100.0) {
        m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        std::fill(m_bufferL.begin(), m_bufferL.end(), 0.0f);
        std::fill(m_bufferR.begin(), m_bufferR.end(), 0.0f);
        m_writePos = 0;
        m_lfoPhase = 0.0f;
    }

    void setParameters(uint8_t del, uint8_t dep, uint8_t spd, uint8_t mix, uint8_t wid) {
        m_delParam = del;
        m_depParam = dep;
        m_spdParam = spd;
        m_mixParam = mix;
        m_widParam = wid;

        // Base delay: 1.0ms to 25.0ms
        m_baseDelaySamples = (0.001f + (del / 127.0f) * 0.024f) * static_cast<float>(m_sampleRate);

        // Modulation depth: up to 5.0ms swing
        m_modDepthSamples = (dep / 127.0f) * 0.005f * static_cast<float>(m_sampleRate);

        // LFO speed: 0.1 Hz to 8.0 Hz exponential curve
        float normSpd = spd / 127.0f;
        m_lfoFreq = 0.1f * std::pow(80.0f, normSpd);

        // Mix: 0.0 (Dry) to 1.0 (Wet)
        m_wetMix = mix / 127.0f;

        // Stereo width: 0 phase to 180 degree phase difference
        m_stereoSpread = (wid / 127.0f) * 3.14159265358979323846f;
    }

    void processStereo(const float* inL, const float* inR,
                       float* outL, float* outR, size_t numFrames) {
        const float lfoPhaseInc = 2.0f * 3.14159265358979323846f * m_lfoFreq / static_cast<float>(m_sampleRate);

        for (size_t i = 0; i < numFrames; ++i) {
            float inSampleL = inL ? inL[i] : 0.0f;
            float inSampleR = inR ? inR[i] : 0.0f;

            // Write into ring buffers
            m_bufferL[m_writePos] = inSampleL;
            m_bufferR[m_writePos] = inSampleR;

            // Calculate modulated delay times for Left and Right channels
            float modL = std::sin(m_lfoPhase);
            float modR = std::sin(m_lfoPhase + m_stereoSpread);

            float delayL = m_baseDelaySamples + m_modDepthSamples * modL;
            float delayR = m_baseDelaySamples + m_modDepthSamples * modR;

            // Hermite/linear interpolation tap
            float delayedL = readInterpolated(m_bufferL, static_cast<float>(m_writePos) - delayL);
            float delayedR = readInterpolated(m_bufferR, static_cast<float>(m_writePos) - delayR);

            // Mix stage
            outL[i] = (1.0f - m_wetMix) * inSampleL + m_wetMix * delayedL;
            outR[i] = (1.0f - m_wetMix) * inSampleR + m_wetMix * delayedR;

            // Advance buffer cursor & LFO phase
            m_writePos = (m_writePos + 1) % kMaxDelaySamples;
            m_lfoPhase += lfoPhaseInc;
            if (m_lfoPhase >= 2.0f * 3.14159265358979323846f) {
                m_lfoPhase -= 2.0f * 3.14159265358979323846f;
            }
        }
    }

private:
    static constexpr size_t kMaxDelaySamples = 4096;

    inline float readInterpolated(const std::vector<float>& buffer, float readIndex) {
        while (readIndex < 0.0f) readIndex += static_cast<float>(kMaxDelaySamples);
        size_t i0 = static_cast<size_t>(readIndex) % kMaxDelaySamples;
        size_t i1 = (i0 + 1) % kMaxDelaySamples;
        float frac = readIndex - std::floor(readIndex);
        return buffer[i0] + frac * (buffer[i1] - buffer[i0]);
    }

    double m_sampleRate = 44100.0;
    std::vector<float> m_bufferL;
    std::vector<float> m_bufferR;
    size_t m_writePos = 0;

    uint8_t m_delParam = 32;
    uint8_t m_depParam = 64;
    uint8_t m_spdParam = 48;
    uint8_t m_mixParam = 64;
    uint8_t m_widParam = 64;

    float m_baseDelaySamples = 441.0f;
    float m_modDepthSamples = 88.0f;
    float m_lfoFreq = 1.0f;
    float m_lfoPhase = 0.0f;
    float m_stereoSpread = 1.57f;
    float m_wetMix = 0.5f;
};

} // namespace monomachine
