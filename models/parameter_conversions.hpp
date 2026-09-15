#pragma once

#include <string>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace monomachine {

/**
 * @brief Handles parameter conversion, knob-to-frequency mappings,
 * and Monomachine LCD screen formatters for every knob across all pages.
 */
class ParameterConversions {
public:
    // ==========================================
    // 1. FREQUENCY & PITCH CONVERSIONS
    // ==========================================

    /**
     * @brief Converts MIDI note number (0..127) plus fine tune (-64..+63) to frequency in Hz.
     */
    static float midiNoteToFrequency(uint8_t note, uint8_t tune = 64) {
        float semitoneOffset = (static_cast<float>(tune) - 64.0f) / 64.0f * 12.0f;
        float totalPitch = static_cast<float>(note) + semitoneOffset;
        return 440.0f * std::pow(2.0f, (totalPitch - 69.0f) / 12.0f);
    }

    /**
     * @brief Base / Width filter frequency mapping (20 Hz .. 20 kHz log curve).
     */
    static float filterValueToHz(uint8_t val) {
        float norm = std::clamp(static_cast<float>(val) / 127.0f, 0.0f, 1.0f);
        return 20.0f * std::pow(1000.0f, norm);
    }

    /**
     * @brief 1-Band Parametric EQ frequency mapping (40 Hz .. 16 kHz).
     */
    static float eqFrequencyToHz(uint8_t val) {
        float norm = std::clamp(static_cast<float>(val) / 127.0f, 0.0f, 1.0f);
        return 40.0f * std::pow(400.0f, norm);
    }

    /**
     * @brief EQ Gain in dB (-12 dB .. +12 dB).
     */
    static float eqGainTodB(uint8_t val) {
        return (static_cast<float>(val) - 64.0f) / 64.0f * 12.0f;
    }

    // ==========================================
    // 2. FM RATIO MAPPINGS (FM+ DYNAMIC)
    // ==========================================

    /**
     * @brief 1FRQ: Continuous linear ratio multiplier (0.0 to ~8.0, 16 -> 1.0).
     */
    static float fm1FrqToRatio(uint8_t val) {
        return static_cast<float>(val) / 16.0f;
    }

    /**
     * @brief 2FRQ: Continuous exponential ratio multiplier (0.0625 to 16.0, 32 -> 1.0, 42 -> ~1.33).
     */
    static float fm2FrqToRatio(uint8_t val) {
        return std::pow(2.0f, (static_cast<float>(val) - 32.0f) / 24.0f);
    }

    // ==========================================
    // 3. TIME CONVERSIONS
    // ==========================================

    /**
     * @brief Envelope attack time in milliseconds (0 ms .. 5000 ms).
     */
    static float attackValueToMs(uint8_t val) {
        float norm = static_cast<float>(val) / 127.0f;
        return 1.0f + 5000.0f * (norm * norm);
    }

    /**
     * @brief Envelope decay/release time in milliseconds (5 ms .. 10000 ms).
     */
    static float decayValueToMs(uint8_t val) {
        float norm = static_cast<float>(val) / 127.0f;
        return 5.0f + 10000.0f * (norm * norm);
    }

    /**
     * @brief Track Delay Time in milliseconds (1 ms .. 750 ms).
     */
    static float delayTimeToMs(uint8_t val) {
        return 1.0f + (static_cast<float>(val) / 127.0f) * 750.0f;
    }

    // ==========================================
    // 4. SCREEN LCD VALUE FORMATTERS
    // ==========================================

    /**
     * @brief Format parameter value to match the Monomachine LCD screen shown in gui.jpg.
     */
    static std::string formatLcdValue(const std::string& paramName, uint8_t val) {
        char buf[32];
        if (paramName == "1FRQ") {
            snprintf(buf, sizeof(buf), "%.1f", fm1FrqToRatio(val));
            return buf;
        } else if (paramName == "2FRQ") {
            snprintf(buf, sizeof(buf), "%.2f", fm2FrqToRatio(val));
            return buf;
        } else if (paramName == "TUNE" || paramName == "BOFS" || paramName == "WOFS" ||
                   paramName == "1FEN" || paramName == "EQG"  || paramName == "PAN") {
            int bip = static_cast<int>(val) - 64;
            snprintf(buf, sizeof(buf), "%d", bip);
            return buf;
        } else if (paramName == "MULT") {
            // LFO multipliers: 1x, 2x, 4x, 8x, 16x, 32x, 64x
            int mults[] = {1, 2, 4, 8, 16, 32, 64};
            int idx = std::clamp(static_cast<int>(val / 18), 0, 6);
            snprintf(buf, sizeof(buf), "%dx", mults[idx]);
            return buf;
        } else if (paramName == "TRIG") {
            const char* trigs[] = {"FREE", "TRIG", "HOLD", "ONE"};
            int idx = std::clamp(static_cast<int>(val / 32), 0, 3);
            return trigs[idx];
        } else if (paramName == "WAVE") {
            const char* waves[] = {"TRI", "SAW", "SQR", "EXP", "RND"};
            int idx = std::clamp(static_cast<int>(val / 26), 0, 4);
            return waves[idx];
        } else if (paramName == "PAGE") {
            const char* pages[] = {"PTCH", "SYN", "AMP", "FLT", "FX"};
            int idx = std::clamp(static_cast<int>(val / 26), 0, 4);
            return pages[idx];
        } else if (paramName == "DEST") {
            return "2OCT";
        }
        snprintf(buf, sizeof(buf), "%d", val);
        return buf;
    }
};

} // namespace monomachine
