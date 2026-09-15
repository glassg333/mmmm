#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace monomachine {

/**
 * @brief Elektron Monomachine Base-Width Multi-Mode Resonant Filter.
 * 
 * Hardware Architecture:
 * - BASE defines High-Pass Cutoff (0..127).
 * - WIDTH defines Low-Pass Cutoff span relative to BASE (LP Cutoff = BASE + WIDTH).
 * - HPQ / LPQ set the resonance/peak damping of the respective filters.
 * - Modulations from Filter Envelope (ATK, DEC, BOFS, WOFS) offset Base and Width dynamically.
 * - Formed by a 2-pole resonant High-Pass filter in series with a 2-pole resonant Low-Pass filter.
 */
class MonomachineFilter {
public:
    MonomachineFilter() = default;

    void reset(double sampleRate = 44100.0) {
        m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        m_hpL.reset();
        m_hpR.reset();
        m_lpL.reset();
        m_lpR.reset();
        m_envStage = EnvStage::Idle;
        m_envValue = 0.0f;
    }

    void setParameters(uint8_t base, uint8_t width, uint8_t hpq, uint8_t lpq,
                       uint8_t attack, uint8_t decay, int8_t bofs, int8_t wofs) {
        m_baseParam = base;
        m_widthParam = width;
        m_hpqParam = hpq;
        m_lpqParam = lpq;
        m_attackParam = attack;
        m_decayParam = decay;
        m_bofsParam = bofs;
        m_wofsParam = wofs;

        updateCoefficients();
    }

    void triggerEnvelope() {
        m_envStage = EnvStage::Attack;
    }

    void releaseEnvelope() {
        m_envStage = EnvStage::Decay;
    }

    void processStereo(const float* inL, const float* inR,
                       float* outL, float* outR, size_t numFrames) {
        for (size_t i = 0; i < numFrames; ++i) {
            stepEnvelope();

            float left = inL ? inL[i] : 0.0f;
            float right = inR ? inR[i] : 0.0f;

            // Series routing: Input -> High-Pass (BASE) -> Low-Pass (BASE + WIDTH)
            left = m_hpL.process(left);
            right = m_hpR.process(right);

            left = m_lpL.process(left);
            right = m_lpR.process(right);

            outL[i] = left;
            outR[i] = right;
        }
    }

private:
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() {
            z1 = 0.0f;
            z2 = 0.0f;
        }

        inline float process(float in) {
            float out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return out;
        }

        void setHighPass(float cutoffHz, float q, double sampleRate) {
            float omega = 2.0f * 3.14159265358979323846f * std::clamp(cutoffHz, 10.0f, static_cast<float>(sampleRate * 0.49));
            float w0 = omega / static_cast<float>(sampleRate);
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * std::max(q, 0.1f));

            float a0 = 1.0f + alpha;
            b0 = ((1.0f + cosw0) / 2.0f) / a0;
            b1 = (-(1.0f + cosw0)) / a0;
            b2 = ((1.0f + cosw0) / 2.0f) / a0;
            a1 = (-2.0f * cosw0) / a0;
            a2 = (1.0f - alpha) / a0;
        }

        void setLowPass(float cutoffHz, float q, double sampleRate) {
            float omega = 2.0f * 3.14159265358979323846f * std::clamp(cutoffHz, 10.0f, static_cast<float>(sampleRate * 0.49));
            float w0 = omega / static_cast<float>(sampleRate);
            float cosw0 = std::cos(w0);
            float sinw0 = std::sin(w0);
            float alpha = sinw0 / (2.0f * std::max(q, 0.1f));

            float a0 = 1.0f + alpha;
            b0 = ((1.0f - cosw0) / 2.0f) / a0;
            b1 = (1.0f - cosw0) / a0;
            b2 = ((1.0f - cosw0) / 2.0f) / a0;
            a1 = (-2.0f * cosw0) / a0;
            a2 = (1.0f - alpha) / a0;
        }
    };

    enum class EnvStage { Idle, Attack, Decay };

    void stepEnvelope() {
        if (m_envStage == EnvStage::Attack) {
            float rate = 1.0f / (0.001f + (static_cast<float>(m_attackParam) / 127.0f) * static_cast<float>(m_sampleRate) * 0.5f);
            m_envValue += rate;
            if (m_envValue >= 1.0f) {
                m_envValue = 1.0f;
                m_envStage = EnvStage::Decay;
            }
            updateCoefficients();
        } else if (m_envStage == EnvStage::Decay) {
            float decayFactor = std::exp(-1.0f / (0.005f + (static_cast<float>(m_decayParam) / 127.0f) * static_cast<float>(m_sampleRate) * 1.5f));
            m_envValue *= decayFactor;
            if (m_envValue < 0.0001f) {
                m_envValue = 0.0f;
                m_envStage = EnvStage::Idle;
            }
            updateCoefficients();
        }
    }

    void updateCoefficients() {
        // Monomachine exponential frequency curve approximation for 0..127
        auto mnmFreq = [](float val) {
            float norm = std::clamp(val / 127.0f, 0.0f, 1.0f);
            return 20.0f * std::pow(1000.0f, norm); // ~20 Hz to 20 kHz
        };

        float effectiveBase = static_cast<float>(m_baseParam) + m_envValue * (m_bofsParam / 127.0f) * 64.0f;
        float effectiveWidth = static_cast<float>(m_widthParam) + m_envValue * (m_wofsParam / 127.0f) * 64.0f;
        effectiveBase = std::clamp(effectiveBase, 0.0f, 127.0f);
        effectiveWidth = std::clamp(effectiveWidth, 0.0f, 127.0f);

        float hpFreq = mnmFreq(effectiveBase);
        float lpFreq = mnmFreq(std::min(127.0f, effectiveBase + effectiveWidth));

        // Q mapping (0.5 to ~12.0)
        float hpQ = 0.707f + (static_cast<float>(m_hpqParam) / 127.0f) * 8.0f;
        float lpQ = 0.707f + (static_cast<float>(m_lpqParam) / 127.0f) * 8.0f;

        m_hpL.setHighPass(hpFreq, hpQ, m_sampleRate);
        m_hpR.setHighPass(hpFreq, hpQ, m_sampleRate);

        m_lpL.setLowPass(lpFreq, lpQ, m_sampleRate);
        m_lpR.setLowPass(lpFreq, lpQ, m_sampleRate);
    }

    double m_sampleRate = 44100.0;
    uint8_t m_baseParam = 0;
    uint8_t m_widthParam = 127;
    uint8_t m_hpqParam = 0;
    uint8_t m_lpqParam = 0;
    uint8_t m_attackParam = 0;
    uint8_t m_decayParam = 64;
    int8_t m_bofsParam = 0;
    int8_t m_wofsParam = 0;

    EnvStage m_envStage = EnvStage::Idle;
    float m_envValue = 0.0f;

    Biquad m_hpL, m_hpR;
    Biquad m_lpL, m_lpR;
};

} // namespace monomachine
