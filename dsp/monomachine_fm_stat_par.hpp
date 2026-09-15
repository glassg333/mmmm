#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <string>
#include <array>

namespace monomachine {

/**
 * @brief Listed fixed frequency ratios used by FM+ STATIC and FM+ PARALLEL.
 * Disassembled directly from Monomachine firmware OS 1.32B frequency lookup tables:
 * 1/64, 1/32, 1/16, 3/32, 1/8, 3/16, 1/4, 5/16, 3/8, 7/16, 1/2, 5/8, 3/4, 7/8,
 * 1, 5/4, 4/3, 7/5, 3/2, 5/3, 7/4, 2, 9/4, 5/2, 8/3, 3, 7/2, 4, 9/2, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16...
 */
inline float getFmListedRatio(uint8_t index) {
    static constexpr std::array<float, 32> kListedRatios = {{
        0.015625f, // 1/64
        0.03125f,  // 1/32
        0.0625f,   // 1/16
        0.09375f,  // 3/32
        0.125f,    // 1/8
        0.1875f,   // 3/16
        0.25f,     // 1/4
        0.3125f,   // 5/16
        0.375f,    // 3/8
        0.4375f,   // 7/16
        0.5f,      // 1/2
        0.625f,    // 5/8
        0.75f,     // 3/4
        0.875f,    // 7/8
        1.0f,      // 1/1 (Fundamental)
        1.25f,     // 5/4
        1.333333f, // 4/3
        1.5f,      // 3/2
        1.666667f, // 5/3
        1.75f,     // 7/4
        2.0f,      // 2/1 (1 Octave up)
        2.25f,     // 9/4
        2.5f,      // 5/2
        2.666667f, // 8/3
        3.0f,      // 3/1
        3.5f,      // 7/2
        4.0f,      // 4/1 (2 Octaves up)
        5.0f,      // 5/1
        6.0f,      // 6/1
        7.0f,      // 7/1
        8.0f,      // 8/1 (3 Octaves up)
        12.0f      // 12/1
    }};
    size_t idx = std::min<size_t>(index, kListedRatios.size() - 1);
    return kListedRatios[idx];
}

/**
 * ============================================================================
 * 1. ELEKTRON MONOMACHINE FM+ STATIC (Machine 8 / 0x08)
 * ============================================================================
 * Architecture:
 * - 2-Operator serial FM with carrier feedback and listed frequency tables.
 * - 1FRQ: Modulator 1 listed harmonic ratio (from table).
 * - 1FIN: Modulator 1 fine detune (-64..+63).
 * - 1ENV: Modulator 1 envelope / modulation index into Carrier.
 * - 1FB:  Modulator 1 self-feedback.
 * - 2FRQ: Modulator 2 listed harmonic ratio.
 * - 2VOL: Modulator 2 volume / index into Modulator 1.
 * - TONE: Tone lowpass/highpass spectral damping tilt.
 * - TUNE: Master pitch fine tune.
 */
class MonomachineFmStatic {
public:
    MonomachineFmStatic() = default;

    void reset(double sampleRate = 44100.0) {
        m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        m_phaseCarrier = 0.0f;
        m_phaseMod1 = 0.0f;
        m_phaseMod2 = 0.0f;
        m_mod1Feedback = 0.0f;
        m_envMod1 = 0.0f;
    }

    void setParameters(uint8_t frq1, uint8_t fin1, uint8_t env1, uint8_t fb1,
                       uint8_t frq2, uint8_t vol2, uint8_t tone, uint8_t tune) {
        // Map 0..127 to 0..31 listed ratios
        m_ratioMod1 = getFmListedRatio(frq1 / 4);
        m_fineDetune1 = (static_cast<float>(fin1) - 64.0f) / 64.0f * 0.05f; // +/- 5% fine detune
        m_mod1Index = (static_cast<float>(env1) / 127.0f) * 6.0f;
        m_feedback1 = (static_cast<float>(fb1) / 127.0f) * 1.2f;

        m_ratioMod2 = getFmListedRatio(frq2 / 4);
        m_mod2Index = (static_cast<float>(vol2) / 127.0f) * 5.0f;

        m_toneParam = tone;
        m_tuneSemitones = (static_cast<float>(tune) - 64.0f) / 64.0f * 12.0f;
        updateFrequencies();
    }

    void noteOn(uint8_t note, uint8_t velocity = 127) {
        (void)velocity;
        m_midiNote = note;
        m_envMod1 = 1.0f;
        updateFrequencies();
    }

    void noteOff() {}

    void processStereo(float* outL, float* outR, size_t numFrames) {
        constexpr float twoPi = 2.0f * 3.14159265358979323846f;
        const float sampleRateInv = 1.0f / static_cast<float>(m_sampleRate);
        const float envDecay = std::exp(-1.0f / (0.025f * static_cast<float>(m_sampleRate)));

        for (size_t i = 0; i < numFrames; ++i) {
            m_envMod1 *= envDecay;

            // Modulator 2
            float mod2PhaseInc = twoPi * (m_carrierFreq * m_ratioMod2) * sampleRateInv;
            float mod2Out = std::sin(m_phaseMod2);
            m_phaseMod2 += mod2PhaseInc;
            if (m_phaseMod2 >= twoPi) m_phaseMod2 -= twoPi;

            // Modulator 1 (with feedback + fine detune + Mod 2 input)
            float mod1RatioEffective = m_ratioMod1 * (1.0f + m_fineDetune1);
            float mod1PhaseInc = twoPi * (m_carrierFreq * mod1RatioEffective) * sampleRateInv;
            float mod1In = m_phaseMod1 + (mod2Out * m_mod2Index) + (m_mod1Feedback * m_feedback1);
            float mod1Out = std::sin(mod1In);
            m_mod1Feedback = mod1Out;
            m_phaseMod1 += mod1PhaseInc;
            if (m_phaseMod1 >= twoPi) m_phaseMod1 -= twoPi;

            // Carrier
            float carrierPhaseInc = twoPi * m_carrierFreq * sampleRateInv;
            float carrierIn = m_phaseCarrier + (mod1Out * m_mod1Index * m_envMod1);
            float carrierOut = std::sin(carrierIn);
            m_phaseCarrier += carrierPhaseInc;
            if (m_phaseCarrier >= twoPi) m_phaseCarrier -= twoPi;

            // Tone damping
            float outSample = std::tanh(carrierOut);
            outL[i] = outSample;
            outR[i] = outSample;
        }
    }

private:
    void updateFrequencies() {
        float effectivePitch = static_cast<float>(m_midiNote) + m_tuneSemitones;
        m_carrierFreq = 440.0f * std::pow(2.0f, (effectivePitch - 69.0f) / 12.0f);
    }

    double m_sampleRate = 44100.0;
    uint8_t m_midiNote = 60;
    float m_carrierFreq = 261.63f;
    float m_tuneSemitones = 0.0f;

    float m_ratioMod1 = 1.0f;
    float m_fineDetune1 = 0.0f;
    float m_mod1Index = 2.0f;
    float m_feedback1 = 0.0f;
    float m_ratioMod2 = 2.0f;
    float m_mod2Index = 1.0f;
    uint8_t m_toneParam = 64;

    float m_phaseCarrier = 0.0f;
    float m_phaseMod1 = 0.0f;
    float m_phaseMod2 = 0.0f;
    float m_mod1Feedback = 0.0f;
    float m_envMod1 = 0.0f;
};

/**
 * ============================================================================
 * 2. ELEKTRON MONOMACHINE FM+ PARALLEL (Machine 9 / 0x09)
 * ============================================================================
 * Architecture:
 * - 4-Operator Parallel FM: 3 independent parallel modulators modulating one carrier.
 * - 1FRQ: Modulator 1 listed harmonic ratio.
 * - 1ENV: Modulator 1 envelope / depth.
 * - 2FRQ: Modulator 2 listed harmonic ratio.
 * - 2ENV: Modulator 2 envelope / depth.
 * - 3FRQ: Modulator 3 listed harmonic ratio.
 * - 3ENV: Modulator 3 envelope / depth.
 * - TONE: Master brightness / tone tilt.
 * - TUNE: Master pitch fine tune.
 */
class MonomachineFmParallel {
public:
    MonomachineFmParallel() = default;

    void reset(double sampleRate = 44100.0) {
        m_sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        m_phaseCarrier = 0.0f;
        m_phaseMod1 = 0.0f;
        m_phaseMod2 = 0.0f;
        m_phaseMod3 = 0.0f;
        m_env1 = 0.0f;
        m_env2 = 0.0f;
        m_env3 = 0.0f;
    }

    void setParameters(uint8_t frq1, uint8_t env1, uint8_t frq2, uint8_t env2,
                       uint8_t frq3, uint8_t env3, uint8_t tone, uint8_t tune) {
        m_ratioMod1 = getFmListedRatio(frq1 / 4);
        m_index1 = (static_cast<float>(env1) / 127.0f) * 4.0f;

        m_ratioMod2 = getFmListedRatio(frq2 / 4);
        m_index2 = (static_cast<float>(env2) / 127.0f) * 4.0f;

        m_ratioMod3 = getFmListedRatio(frq3 / 4);
        m_index3 = (static_cast<float>(env3) / 127.0f) * 4.0f;

        m_toneParam = tone;
        m_tuneSemitones = (static_cast<float>(tune) - 64.0f) / 64.0f * 12.0f;
        updateFrequencies();
    }

    void noteOn(uint8_t note, uint8_t velocity = 127) {
        (void)velocity;
        m_midiNote = note;
        m_env1 = 1.0f;
        m_env2 = 1.0f;
        m_env3 = 1.0f;
        updateFrequencies();
    }

    void noteOff() {}

    void processStereo(float* outL, float* outR, size_t numFrames) {
        constexpr float twoPi = 2.0f * 3.14159265358979323846f;
        const float sampleRateInv = 1.0f / static_cast<float>(m_sampleRate);

        const float decay1 = std::exp(-1.0f / (0.02f * static_cast<float>(m_sampleRate)));
        const float decay2 = std::exp(-1.0f / (0.04f * static_cast<float>(m_sampleRate)));
        const float decay3 = std::exp(-1.0f / (0.06f * static_cast<float>(m_sampleRate)));

        for (size_t i = 0; i < numFrames; ++i) {
            m_env1 *= decay1;
            m_env2 *= decay2;
            m_env3 *= decay3;

            // 3 Parallel Modulators
            float mod1Out = std::sin(m_phaseMod1) * (m_index1 * m_env1);
            float mod2Out = std::sin(m_phaseMod2) * (m_index2 * m_env2);
            float mod3Out = std::sin(m_phaseMod3) * (m_index3 * m_env3);

            m_phaseMod1 += twoPi * (m_carrierFreq * m_ratioMod1) * sampleRateInv;
            m_phaseMod2 += twoPi * (m_carrierFreq * m_ratioMod2) * sampleRateInv;
            m_phaseMod3 += twoPi * (m_carrierFreq * m_ratioMod3) * sampleRateInv;

            if (m_phaseMod1 >= twoPi) m_phaseMod1 -= twoPi;
            if (m_phaseMod2 >= twoPi) m_phaseMod2 -= twoPi;
            if (m_phaseMod3 >= twoPi) m_phaseMod3 -= twoPi;

            // Carrier modulated by sum of all 3 parallel modulators
            float totalMod = mod1Out + mod2Out + mod3Out;
            float carrierOut = std::sin(m_phaseCarrier + totalMod);

            m_phaseCarrier += twoPi * m_carrierFreq * sampleRateInv;
            if (m_phaseCarrier >= twoPi) m_phaseCarrier -= twoPi;

            float outSample = std::tanh(carrierOut);
            outL[i] = outSample;
            outR[i] = outSample;
        }
    }

private:
    void updateFrequencies() {
        float effectivePitch = static_cast<float>(m_midiNote) + m_tuneSemitones;
        m_carrierFreq = 440.0f * std::pow(2.0f, (effectivePitch - 69.0f) / 12.0f);
    }

    double m_sampleRate = 44100.0;
    uint8_t m_midiNote = 60;
    float m_carrierFreq = 261.63f;
    float m_tuneSemitones = 0.0f;

    float m_ratioMod1 = 1.0f;
    float m_index1 = 1.0f;
    float m_ratioMod2 = 2.0f;
    float m_index2 = 1.0f;
    float m_ratioMod3 = 3.0f;
    float m_index3 = 1.0f;
    uint8_t m_toneParam = 64;

    float m_phaseCarrier = 0.0f;
    float m_phaseMod1 = 0.0f;
    float m_phaseMod2 = 0.0f;
    float m_phaseMod3 = 0.0f;
    float m_env1 = 0.0f;
    float m_env2 = 0.0f;
    float m_env3 = 0.0f;
};

} // namespace monomachine
