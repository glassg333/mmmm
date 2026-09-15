#pragma once

#include "machine_definitions.hpp"
#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace monomachine {

/**
 * Standard Monomachine 8-parameter pages:
 * Page 1: Synthesis / Machine Parameters (A..H)
 * Page 2: Amplification
 * Page 3: Filter (Base-Width Multi-Mode)
 * Page 4: Effects (Track EQ, SRR, Delay)
 * Page 5: LFO 1
 * Page 6: LFO 2
 * Page 7: LFO 3
 */

struct PageParameterInfo {
    std::string name;       // 4-char abbreviation
    std::string label;      // Full display title
    std::string unit;       // Display unit
    uint8_t defaultVal;
    bool isBipolar;
};

inline std::array<PageParameterInfo, 8> getAmpPageDefinition() {
    return {{
        {"ATK",  "Attack Time",      "ms",  0,  false},
        {"HOLD", "Hold Time",        "ms",  0,  false},
        {"DEC",  "Decay Time",       "ms",  64, false},
        {"REL",  "Release Time",     "ms",  64, false},
        {"DIST", "Distortion Drive", "",    0,  false},
        {"VOL",  "Amp Volume",       "",    64, false},
        {"PAN",  "Stereo Pan",       "",    64, true},
        {"PORT", "Portamento Time",  "ms",  0,  false}
    }};
}

inline std::array<PageParameterInfo, 8> getFilterPageDefinition() {
    return {{
        {"BASE", "Filter Base (HPF Cutoff)",  "Hz", 0,   false},
        {"WDTH", "Filter Width (LPF Cutoff)", "Hz", 127, false},
        {"HPQ",  "High-Pass Resonance",       "",   0,   false},
        {"LPQ",  "Low-Pass Resonance",        "",   0,   false},
        {"ATK",  "Filter Envelope Attack",    "ms", 0,   false},
        {"DEC",  "Filter Envelope Decay",     "ms", 93,  false},
        {"BOFS", "Base Offset Env Depth",     "",   64,  true},
        {"WOFS", "Width Offset Env Depth",    "",   64,  true}
    }};
}

inline std::array<PageParameterInfo, 8> getEffectsPageDefinition() {
    return {{
        {"EQF",  "EQ Center Frequency",      "Hz", 64,  false},
        {"EQG",  "EQ Gain Boost/Cut",        "dB", 64,  true},
        {"SRR",  "Sample Rate Reduction",    "",   0,   false},
        {"DTIM", "Delay Time",               "ms", 64,  false},
        {"DSND", "Delay Send Level",         "",   0,   false},
        {"DFB",  "Delay Feedback",           "",   28,  false},
        {"DBAS", "Delay Filter Base (HPF)",  "Hz", 0,   false},
        {"DWID", "Delay Filter Width (LPF)", "Hz", 127, false}
    }};
}

inline std::array<PageParameterInfo, 8> getLfoPageDefinition(int lfoIndex = 1) {
    (void)lfoIndex;
    return {{
        {"PAGE", "Target Page (PTCH/SYN/AMP/FLT/FX)", "", 0,  false},
        {"DEST", "Target Parameter Destination",      "", 0,  false},
        {"TRIG", "Trigger Mode (FREE/TRIG/HOLD/ONE)", "", 0,  false},
        {"WAVE", "Waveform (TRI/SAW/SQR/EXP/RND)",    "", 0,  false},
        {"MULT", "Multiplier (1x..64x)",              "", 0,  false},
        {"SPD",  "LFO Speed Rate",                    "", 64, true},
        {"INTL", "Interlace / Phase Sync",            "", 0,  false},
        {"DPTH", "Modulation Depth",                  "", 0,  true}
    }};
}

} // namespace monomachine
