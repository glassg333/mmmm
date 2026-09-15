#pragma once

#include "monomachine_filter.hpp"
#include "monomachine_chorus.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace monomachine {

/**
 * @brief Mode for FX triggering:
 * - FreeRunning: Gate is continuously held open (100% audio throughput, envelope bypassed).
 * - MidiTriggered: Envelopes strictly require MIDI Note-On to open gate and sweep filter.
 */
enum class FxGateMode : uint8_t {
    FreeRunning,   // Works continuously without MIDI (traditional insert FX)
    MidiTriggered  // Strictly gated & shaped by incoming MIDI notes (rhythmic/sidechain FX)
};

/**
 * @brief Complete native Monomachine track voice routing chain.
 * Matches the layout and processing order of Monomachine OS 1.32B:
 * 
 * Machine/FX -> Distortion/SRR -> Filter (Base/Width) -> EQ -> Amp Env/Pan -> Delay -> Master
 */
class MonomachineVoiceChain {
public:
    MonomachineVoiceChain() = default;

    void reset(double sampleRate = 44100.0) {
        m_sampleRate = sampleRate;
        m_filter.reset(sampleRate);
        m_chorus.reset(sampleRate);
        m_delayBufferL.assign(kMaxDelaySamples, 0.0f);
        m_delayBufferR.assign(kMaxDelaySamples, 0.0f);
        m_delayWritePos = 0;
        m_ampEnvStage = AmpEnvStage::Idle;
        m_ampEnvVal = 0.0f;
    }

    MonomachineFilter& filter() noexcept { return m_filter; }
    MonomachineChorus& chorus() noexcept { return m_chorus; }

    void setFxGateMode(FxGateMode mode) noexcept {
        m_fxGateMode = mode;
        if (m_fxGateMode == FxGateMode::FreeRunning) {
            m_ampEnvVal = 1.0f; // Gate held open continuously
        }
    }

    FxGateMode fxGateMode() const noexcept { return m_fxGateMode; }

    void noteOn(uint8_t note, uint8_t velocity = 127) {
        (void)note;
        (void)velocity;
        m_filter.triggerEnvelope();
        m_ampEnvStage = AmpEnvStage::Attack;
    }

    void noteOff() {
        m_filter.releaseEnvelope();
        m_ampEnvStage = AmpEnvStage::Release;
    }

    void setAmpEnvelope(uint8_t atk, uint8_t hold, uint8_t dec, uint8_t rel) {
        m_atkParam = atk;
        m_holdParam = hold;
        m_decParam = dec;
        m_relParam = rel;
    }

    void setDistortion(uint8_t dist) { m_distortion = dist; }
    void setVolume(uint8_t vol) { m_volume = vol / 127.0f; }
    void setPan(uint8_t pan) {
        m_panL = pan <= 64 ? 1.0f : (127.0f - pan) / 63.0f;
        m_panR = pan >= 64 ? 1.0f : pan / 64.0f;
    }

    void setDelay(uint8_t time, uint8_t send, uint8_t feedback) {
        m_delayTimeSamples = std::clamp(static_cast<size_t>((time / 127.0f) * static_cast<float>(m_sampleRate) * 0.75f),
                                        static_cast<size_t>(1), kMaxDelaySamples - 1);
        m_delaySend = send / 127.0f;
        m_delayFeedback = (feedback / 127.0f) * 0.95f;
    }

    void processBlock(const float* inL, const float* inR,
                      float* outL, float* outR, size_t numFrames) {
        // 1. FX Stage (e.g. Chorus when active)
        m_chorus.processStereo(inL, inR, outL, outR, numFrames);

        // 2. Distortion / Saturation Stage
        if (m_distortion > 0) {
            float drive = 1.0f + (m_distortion / 127.0f) * 15.0f;
            for (size_t i = 0; i < numFrames; ++i) {
                outL[i] = std::tanh(outL[i] * drive);
                outR[i] = std::tanh(outR[i] * drive);
            }
        }

        // 3. Multi-Mode Filter Stage (Base-Width 24dB)
        m_filter.processStereo(outL, outR, outL, outR, numFrames);

        // 4. Amp Envelope & Pan Stage
        for (size_t i = 0; i < numFrames; ++i) {
            stepAmpEnvelope();
            float gain = m_volume * m_ampEnvVal;
            outL[i] *= gain * m_panL;
            outR[i] *= gain * m_panR;
        }

        // 5. Delay Stage
        if (m_delaySend > 0.001f) {
            for (size_t i = 0; i < numFrames; ++i) {
                size_t readPos = (m_delayWritePos + kMaxDelaySamples - m_delayTimeSamples) % kMaxDelaySamples;
                float dL = m_delayBufferL[readPos];
                float dR = m_delayBufferR[readPos];

                m_delayBufferL[m_delayWritePos] = outL[i] * m_delaySend + dL * m_delayFeedback;
                m_delayBufferR[m_delayWritePos] = outR[i] * m_delaySend + dR * m_delayFeedback;

                outL[i] += dL * m_delaySend;
                outR[i] += dR * m_delaySend;

                m_delayWritePos = (m_delayWritePos + 1) % kMaxDelaySamples;
            }
        }
    }

private:
    enum class AmpEnvStage { Idle, Attack, Hold, Decay, Release };

    void stepAmpEnvelope() {
        if (m_fxGateMode == FxGateMode::FreeRunning) {
            m_ampEnvVal = 1.0f; // Gate remains wide open, no MIDI required
            return;
        }

        // MidiTriggered envelope progression
        switch (m_ampEnvStage) {
            case AmpEnvStage::Idle:
                m_ampEnvVal = 0.0f;
                break;
            case AmpEnvStage::Attack: {
                float rate = 1.0f / (0.001f + (m_atkParam / 127.0f) * static_cast<float>(m_sampleRate) * 0.5f);
                m_ampEnvVal += rate;
                if (m_ampEnvVal >= 1.0f) {
                    m_ampEnvVal = 1.0f;
                    m_ampEnvStage = AmpEnvStage::Hold;
                    m_holdCounter = static_cast<size_t>((m_holdParam / 127.0f) * m_sampleRate * 0.5f);
                }
                break;
            }
            case AmpEnvStage::Hold:
                if (m_holdCounter > 0) {
                    --m_holdCounter;
                } else {
                    m_ampEnvStage = AmpEnvStage::Decay;
                }
                break;
            case AmpEnvStage::Decay: {
                float decayFactor = std::exp(-1.0f / (0.005f + (m_decParam / 127.0f) * static_cast<float>(m_sampleRate) * 1.5f));
                m_ampEnvVal *= decayFactor;
                if (m_ampEnvVal < 0.001f) {
                    m_ampEnvVal = 0.0f;
                    m_ampEnvStage = AmpEnvStage::Idle;
                }
                break;
            }
            case AmpEnvStage::Release: {
                float relFactor = std::exp(-1.0f / (0.005f + (m_relParam / 127.0f) * static_cast<float>(m_sampleRate) * 1.5f));
                m_ampEnvVal *= relFactor;
                if (m_ampEnvVal < 0.001f) {
                    m_ampEnvVal = 0.0f;
                    m_ampEnvStage = AmpEnvStage::Idle;
                }
                break;
            }
        }
    }

    static constexpr size_t kMaxDelaySamples = 65536;

    double m_sampleRate = 44100.0;
    MonomachineFilter m_filter;
    MonomachineChorus m_chorus;

    FxGateMode m_fxGateMode = FxGateMode::FreeRunning;
    AmpEnvStage m_ampEnvStage = AmpEnvStage::Idle;
    float m_ampEnvVal = 1.0f;
    size_t m_holdCounter = 0;

    uint8_t m_atkParam = 0;
    uint8_t m_holdParam = 64;
    uint8_t m_decParam = 64;
    uint8_t m_relParam = 64;

    uint8_t m_distortion = 0;
    float m_volume = 1.0f;
    float m_panL = 1.0f;
    float m_panR = 1.0f;

    std::vector<float> m_delayBufferL;
    std::vector<float> m_delayBufferR;
    size_t m_delayWritePos = 0;
    size_t m_delayTimeSamples = 22050;
    float m_delaySend = 0.0f;
    float m_delayFeedback = 0.0f;
};

} // namespace monomachine
