#pragma once

#include <cmath>
#include <string>
#include <algorithm>
#include <cstdint>

namespace monomachine {

/**
 * @brief Elektron Monomachine FM+ DYNAMIC (Machine 10) Native Implementation.
 * 
 * Hardware Architecture & Manual Definition:
 * - FM+ Dynamic is the continuous-frequency FM synthesis engine with 2 interacting modulators/operators.
 * - Carrier frequency is determined by MIDI note pitch + TUNE parameter.
 * - 1FRQ (Modulator 1 Linear Frequency): continuous linear frequency ratio multiplier [0.0 .. 16.0], displayed as [0.0 .. 2.0] / fractions.
 * - 1FEN (Modulator 1 Frequency Envelope): bipolar depth & decay for Mod 1 pitch sweep (-64 .. +63).
 * - 1VOL (Modulator 1 Volume / Modulation Index): modulation depth of Modulator 1 into Carrier.
 * - 1VEN (Modulator 1 Volume Envelope): envelope modulation of Mod 1 index over time.
 * - 2FRQ (Modulator 2 Exponential Frequency): continuous exponential ratio multiplier [0.0 .. 32.0], displayed as [0.0 .. 2.0] / fractions.
 * - 2ENV (Modulator 2 Volume & Envelope): modulation depth & decay of Modulator 2 into Modulator 1 / Carrier.
 * - 2FB  (Modulator 2 Feedback): self-modulation feedback loop for Modulator 2, producing rich harmonic sidebands.
 * - TUNE (Master Pitch Tune): fine detune / semitone offset (-64 .. +63).
 */
class MonomachineFmDynamic {
public:
    MonomachineFmDynamic() = default;

    void reset(double sampleRate = 44100.0) {
        m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        m_phaseCarrier = 0.0f;
        m_phaseMod1 = 0.0f;
        m_phaseMod2 = 0.0f;
        m_feedbackMod2 = 0.0f;
        m_env1F = 0.0f;
        m_env1V = 0.0f;
        m_env2V = 0.0f;
        m_gate = false;
    }

    /**
     * @brief Configure parameters from Monomachine 0..127 knob values.
     */
    void setParameters(uint8_t frq1, uint8_t fen1, uint8_t vol1, uint8_t ven1,
                       uint8_t frq2, uint8_t env2, uint8_t fb2, uint8_t tune) {
        m_paramFrq1 = frq1;
        m_paramFen1 = fen1;
        m_paramVol1 = vol1;
        m_paramVen1 = ven1;
        m_paramFrq2 = frq2;
        m_paramEnv2 = env2;
        m_paramFb2  = fb2;
        m_paramTune = tune;

        // 1FRQ: Linear ratio 0.0 to ~8.0 (at 16 -> ~1.0 ratio, at 127 -> ~8.0 ratio)
        m_ratioMod1Linear = (static_cast<float>(frq1) / 16.0f);

        // 1FEN: Bipolar envelope depth (-1.0 to +1.0)
        m_depthFen1 = (static_cast<float>(fen1) - 64.0f) / 64.0f;

        // 1VOL: Mod 1 volume / FM index (0.0 to ~6.0 radians of phase modulation)
        m_baseIndexMod1 = (static_cast<float>(vol1) / 127.0f) * 6.0f;

        // 1VEN: Mod 1 volume envelope depth
        m_depthVen1 = static_cast<float>(ven1) / 127.0f;

        // 2FRQ: Exponential ratio 0.0625 to ~16.0 (at 32 -> ~1.0 ratio)
        float expVal = (static_cast<float>(frq2) - 32.0f) / 24.0f;
        m_ratioMod2Exp = std::pow(2.0f, expVal);

        // 2ENV: Mod 2 envelope & volume index
        m_baseIndexMod2 = (static_cast<float>(env2) / 127.0f) * 5.0f;

        // 2FB: Mod 2 feedback amount (0.0 to ~1.2)
        m_feedbackAmount = (static_cast<float>(fb2) / 127.0f) * 1.2f;

        // TUNE: Master pitch fine offset in semitones (-12 to +12)
        m_tuneSemitones = (static_cast<float>(tune) - 64.0f) / 64.0f * 12.0f;

        updateFrequencies();
    }

    /**
     * @brief Trigger note on event with MIDI note (0..127)
     */
    void noteOn(uint8_t midiNote, uint8_t velocity = 127) {
        m_midiNote = midiNote;
        m_velocity = velocity;
        m_gate = true;

        // Reset envelopes on trigger
        m_env1F = 1.0f;
        m_env1V = 1.0f;
        m_env2V = 1.0f;

        updateFrequencies();
    }

    void noteOff() {
        m_gate = false;
    }

    /**
     * @brief Helper to format knob values to Monomachine screen display string.
     */
    static std::string formatParamValue(int knobIndex, uint8_t val) {
        char buf[32];
        switch (knobIndex) {
            case 0: { // 1FRQ (Linear Ratio e.g. "1.0", "1.33")
                float ratio = val / 16.0f;
                snprintf(buf, sizeof(buf), "%.2f", ratio);
                return buf;
            }
            case 1: { // 1FEN (Bipolar Env Depth -64..+63)
                int bip = static_cast<int>(val) - 64;
                snprintf(buf, sizeof(buf), "%d", bip);
                return buf;
            }
            case 2: // 1VOL (0..127)
                snprintf(buf, sizeof(buf), "%d", val);
                return buf;
            case 3: // 1VEN (0..127)
                snprintf(buf, sizeof(buf), "%d", val);
                return buf;
            case 4: { // 2FRQ (Exponential Ratio e.g. "1.0", "1.33", "2.0")
                float ratio = std::pow(2.0f, (val - 32.0f) / 24.0f);
                snprintf(buf, sizeof(buf), "%.2f", ratio);
                return buf;
            }
            case 5: // 2ENV (0..127)
                snprintf(buf, sizeof(buf), "%d", val);
                return buf;
            case 6: // 2FB (0..127)
                snprintf(buf, sizeof(buf), "%d", val);
                return buf;
            case 7: { // TUNE (-64..+63)
                int bip = static_cast<int>(val) - 64;
                snprintf(buf, sizeof(buf), "%d", bip);
                return buf;
            }
            default:
                snprintf(buf, sizeof(buf), "%d", val);
                return buf;
        }
    }

    /**
     * @brief Process audio block of stereo samples.
     */
    void processStereo(float* outL, float* outR, size_t numFrames) {
        constexpr float twoPi = 2.0f * 3.14159265358979323846f;
        const float sampleRateInv = 1.0f / static_cast<float>(m_sampleRate);

        // Exponential decay envelope step rates
        const float decayRate1F = std::exp(-1.0f / (0.01f * static_cast<float>(m_sampleRate)));
        const float decayRate1V = std::exp(-1.0f / (0.03f * static_cast<float>(m_sampleRate)));
        const float decayRate2V = std::exp(-1.0f / (0.02f * static_cast<float>(m_sampleRate)));

        for (size_t i = 0; i < numFrames; ++i) {
            // Envelope steps
            m_env1F *= decayRate1F;
            m_env1V *= decayRate1V;
            m_env2V *= decayRate2V;

            // Modulator 2 (with feedback)
            float mod2PhaseInc = twoPi * (m_carrierFreq * m_ratioMod2Exp) * sampleRateInv;
            float mod2Sample = std::sin(m_phaseMod2 + m_feedbackMod2 * m_feedbackAmount);
            m_feedbackMod2 = mod2Sample;
            m_phaseMod2 += mod2PhaseInc;
            if (m_phaseMod2 >= twoPi) m_phaseMod2 -= twoPi;

            // Modulator 1 (with linear ratio + pitch envelope modulation + Mod 2 input)
            float mod1RatioEffective = std::max(0.01f, m_ratioMod1Linear + m_depthFen1 * m_env1F);
            float mod1PhaseInc = twoPi * (m_carrierFreq * mod1RatioEffective) * sampleRateInv;
            float mod2Modulation = mod2Sample * (m_baseIndexMod2 * m_env2V);
            float mod1Sample = std::sin(m_phaseMod1 + mod2Modulation);
            m_phaseMod1 += mod1PhaseInc;
            if (m_phaseMod1 >= twoPi) m_phaseMod1 -= twoPi;

            // Carrier (modulated by Modulator 1)
            float carrierPhaseInc = twoPi * m_carrierFreq * sampleRateInv;
            float mod1Modulation = mod1Sample * (m_baseIndexMod1 * (1.0f + m_depthVen1 * (m_env1V - 1.0f)));
            float carrierSample = std::sin(m_phaseCarrier + mod1Modulation);
            m_phaseCarrier += carrierPhaseInc;
            if (m_phaseCarrier >= twoPi) m_phaseCarrier -= twoPi;

            // Soft-saturation to emulate Monomachine 24-bit fixed point headroom
            float output = std::tanh(carrierSample);

            outL[i] = output;
            outR[i] = output;
        }
    }

private:
    void updateFrequencies() {
        // Base carrier frequency from MIDI note + Tune
        float effectivePitch = static_cast<float>(m_midiNote) + m_tuneSemitones;
        m_carrierFreq = 440.0f * std::pow(2.0f, (effectivePitch - 69.0f) / 12.0f);
    }

    double m_sampleRate = 44100.0;
    uint8_t m_midiNote = 60;
    uint8_t m_velocity = 127;
    bool m_gate = false;

    // Parameters
    uint8_t m_paramFrq1 = 16;
    uint8_t m_paramFen1 = 64;
    uint8_t m_paramVol1 = 64;
    uint8_t m_paramVen1 = 64;
    uint8_t m_paramFrq2 = 32;
    uint8_t m_paramEnv2 = 80;
    uint8_t m_paramFb2  = 30;
    uint8_t m_paramTune = 64;

    float m_carrierFreq = 261.63f;
    float m_tuneSemitones = 0.0f;
    float m_ratioMod1Linear = 1.0f;
    float m_depthFen1 = 0.0f;
    float m_baseIndexMod1 = 2.0f;
    float m_depthVen1 = 0.5f;
    float m_ratioMod2Exp = 1.33f;
    float m_baseIndexMod2 = 3.0f;
    float m_feedbackAmount = 0.3f;

    // Phases & feedback state
    float m_phaseCarrier = 0.0f;
    float m_phaseMod1 = 0.0f;
    float m_phaseMod2 = 0.0f;
    float m_feedbackMod2 = 0.0f;

    // Envelopes
    float m_env1F = 0.0f;
    float m_env1V = 0.0f;
    float m_env2V = 0.0f;
};

} // namespace monomachine
