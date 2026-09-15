#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace monomachine {

enum class MachineCategory {
    GND,
    SID,
    SWAVE,
    DPRO,
    FM_PLUS,
    VO,
    FX
};

struct ParameterDef {
    std::string name;       // 4-char abbreviation e.g. "1FRQ", "ATK"
    std::string fullName;   // descriptive e.g. "Operator 1 Frequency"
    uint8_t minVal = 0;
    uint8_t maxVal = 127;
    uint8_t defaultVal = 0;
};

struct MachineDef {
    uint8_t id;
    std::string category;
    std::string name;
    bool isEffect;
    std::array<ParameterDef, 8> synthParams;
};

inline const std::vector<MachineDef>& getAllMachineDefinitions() {
    static const std::vector<MachineDef> kMachines = {
        // GND Family
        {
            0, "GND", "GND-GND", false,
            {{
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0}
            }}
        },
        {
            1, "GND", "GND-SIN", false,
            {{
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"TUNE", "Pitch Fine Tune", 0, 127, 64}
            }}
        },
        {
            2, "GND", "GND-NOIS", false,
            {{
                {"ST", "Stereo Spread", 0, 127, 64}, {"RED", "Sample Rate Reduction", 0, 127, 0},
                {"STON", "Stereo Noise", 0, 127, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"TUNE", "Noise Pitch/Color", 0, 127, 64}
            }}
        },

        // SID Family
        {
            3, "SID", "SID-6581", false,
            {{
                {"PW", "Pulse Width", 0, 127, 64}, {"PWAD", "Pulse Width Attack/Decay", 0, 127, 0},
                {"PWRS", "Pulse Width Reset", 0, 127, 0}, {"WAVE", "Osc Waveform (Tri/Saw/Pulse/Noise)", 0, 127, 0},
                {"MOD", "Modulation Mode (Sync/Ring)", 0, 127, 0}, {"MSRC", "Modulation Source", 0, 127, 0},
                {"MFRQ", "Modulation Frequency", 0, 127, 64}, {"TUNE", "Oscillator Tune", 0, 127, 64}
            }}
        },

        // SWAVE Family
        {
            4, "SWAVE", "SWAVE-SAW", false,
            {{
                {"UNIL", "Unison Level", 0, 127, 0}, {"UNIW", "Unison Width / Detune", 0, 127, 0},
                {"UNIX", "Unison Mix", 0, 127, 0}, {"---", "Unused", 0, 0, 0},
                {"SUBX", "Sub-Osc Mix", 0, 127, 0}, {"SUB1", "Sub-Osc 1 Shape", 0, 127, 0},
                {"SUB2", "Sub-Osc 2 Shape", 0, 127, 0}, {"TUNE", "Oscillator Tune", 0, 127, 64}
            }}
        },
        {
            5, "SWAVE", "SWAVE-PULS", false,
            {{
                {"UNIL", "Unison Level", 0, 127, 0}, {"UNIW", "Unison Width / Detune", 0, 127, 0},
                {"SUB1", "Sub-Osc 1 Shape", 0, 127, 0}, {"SUB2", "Sub-Osc 2 Shape", 0, 127, 0},
                {"PW", "Pulse Width", 0, 127, 64}, {"PWAD", "Pulse Width Attack/Decay", 0, 127, 0},
                {"PWRS", "Pulse Width Reset", 0, 127, 0}, {"TUNE", "Oscillator Tune", 0, 127, 64}
            }}
        },
        {
            14, "SWAVE", "SWAVE-ENS", false,
            {{
                {"PCH2", "Voice 2 Pitch Offset", 0, 127, 64}, {"PCH3", "Voice 3 Pitch Offset", 0, 127, 64},
                {"PCH4", "Voice 4 Pitch Offset", 0, 127, 64}, {"WAVE", "Waveform Shape", 0, 127, 0},
                {"PW", "Pulse Width", 0, 127, 64}, {"CHRL", "Chorus Level", 0, 127, 0},
                {"CHRW", "Chorus Width", 0, 127, 64}, {"TUNE", "Master Pitch Tune", 0, 127, 64}
            }}
        },

        // DPRO Family
        {
            6, "DPRO", "DPRO-WAVE", false,
            {{
                {"WAVE", "Waveform Select", 0, 127, 0}, {"WP", "Wave Position", 0, 127, 0},
                {"WPM", "Wave Pos Modulation", 0, 127, 0}, {"WPRS", "Wave Reset", 0, 127, 0},
                {"SYNC", "Hard Sync Mode", 0, 127, 0}, {"SFRQ", "Sync Frequency", 0, 127, 64},
                {"---", "Unused", 0, 0, 0}, {"TUNE", "Pitch Tune", 0, 127, 64}
            }}
        },
        {
            7, "DPRO", "DPRO-BBOX", false,
            {{
                {"PTCH", "Sample Pitch", 0, 127, 64}, {"STRT", "Sample Start Position", 0, 127, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"RTRG", "Retrigger Count", 0, 127, 0}, {"RTIM", "Retrigger Time", 0, 127, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0}
            }}
        },
        {
            32, "DPRO", "DPRO-DDRW", false,
            {{
                {"WAV1", "Wave Slot 1", 0, 127, 0}, {"MIX", "Waveform Crossfade", 0, 127, 64},
                {"WAV2", "Wave Slot 2", 0, 127, 0}, {"TIME", "Interpolation Time", 0, 127, 0},
                {"BR1", "Harmonic Brightness 1", 0, 127, 64}, {"WID", "Stereo Width", 0, 127, 64},
                {"BR2", "Harmonic Brightness 2", 0, 127, 64}, {"TUNE", "Pitch Tune", 0, 127, 64}
            }}
        },
        {
            33, "DPRO", "DPRO-DENS", false,
            {{
                {"PCH2", "Voice 2 Pitch", 0, 127, 64}, {"PCH3", "Voice 3 Pitch", 0, 127, 64},
                {"PCH4", "Voice 4 Pitch", 0, 127, 64}, {"WAVE", "Wave Slot Select", 0, 127, 0},
                {"---", "Unused", 0, 0, 0}, {"CHRL", "Chorus Level", 0, 127, 0},
                {"CHRW", "Chorus Width", 0, 127, 64}, {"TUNE", "Pitch Tune", 0, 127, 64}
            }}
        },

        // FM+ Family
        {
            8, "FM+", "FM+ STAT", false,
            {{
                {"1FRQ", "Carrier 1 Frequency", 0, 127, 16}, {"1FIN", "Carrier 1 Fine Detune", 0, 127, 64},
                {"1ENV", "Modulator 1 Envelope", 0, 127, 0}, {"1FB", "Carrier 1 Feedback", 0, 127, 0},
                {"2FRQ", "Modulator 2 Frequency", 0, 127, 32}, {"2VOL", "Modulator 2 Level", 0, 127, 64},
                {"TONE", "Harmonic Tone / Filter", 0, 127, 64}, {"TUNE", "Master Pitch Tune", 0, 127, 64}
            }}
        },
        {
            9, "FM+", "FM+ PAR", false,
            {{
                {"1FRQ", "Operator 1 Frequency", 0, 127, 16}, {"1ENV", "Operator 1 Envelope", 0, 127, 64},
                {"2FRQ", "Operator 2 Frequency", 0, 127, 32}, {"2ENV", "Operator 2 Envelope", 0, 127, 64},
                {"3FRQ", "Operator 3 Frequency", 0, 127, 48}, {"3ENV", "Operator 3 Envelope", 0, 127, 64},
                {"TONE", "Harmonic Tone", 0, 127, 64}, {"TUNE", "Master Pitch Tune", 0, 127, 64}
            }}
        },
        {
            10, "FM+", "FM+ DYN", false,
            {{
                {"1FRQ", "Op 1 Frequency Ratio", 0, 127, 16}, {"1FEN", "Op 1 Frequency Env Depth", 0, 127, 0},
                {"1VOL", "Op 1 FM Level", 0, 127, 64}, {"1VEN", "Op 1 Level Env Depth", 0, 127, 0},
                {"2FRQ", "Op 2 Frequency Ratio", 0, 127, 32}, {"2ENV", "Op 2 Env Decay/Depth", 0, 127, 80},
                {"2FB", "Op 2 Feedback Loop", 0, 127, 30}, {"TUNE", "Master Pitch Tune", 0, 127, 64}
            }}
        },

        // VO Family
        {
            11, "VO", "VO-6", false,
            {{
                {"VOC1", "Vocal Formant 1", 0, 127, 64}, {"VOC2", "Vocal Formant 2", 0, 127, 64},
                {"V-SW", "Vocal Formant Sweep", 0, 127, 0}, {"VOIC", "Vowel Voice Character", 0, 127, 0},
                {"CONS", "Consonant Type", 0, 127, 0}, {"CLEN", "Consonant Duration", 0, 127, 32},
                {"CVOL", "Consonant Volume", 0, 127, 64}, {"TUNE", "Voice Pitch Tune", 0, 127, 64}
            }}
        },

        // FX Family (Effects)
        {
            12, "FX", "FX-THRU", true,
            {{
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"INP", "Input Channel Select", 0, 127, 0}
            }}
        },
        {
            13, "FX", "FX-REVERB", true,
            {{
                {"DEC", "Reverb Decay Time", 0, 127, 64}, {"DAMP", "High Frequency Damping", 0, 127, 32},
                {"GATE", "Reverb Gated Decay", 0, 127, 0}, {"MIX", "Wet/Dry Balance", 0, 127, 64},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"INP", "Input Channel Select", 0, 127, 0}
            }}
        },
        {
            15, "FX", "FX-CHORUS", true,
            {{
                {"DEL", "Chorus Base Delay", 0, 127, 32}, {"DEP", "LFO Modulation Depth", 0, 127, 64},
                {"SPD", "LFO Rate / Speed", 0, 127, 48}, {"MIX", "Wet/Dry Balance", 0, 127, 64},
                {"---", "Unused", 0, 0, 0}, {"WID", "Stereo Width Spread", 0, 127, 64},
                {"---", "Unused", 0, 0, 0}, {"INP", "Input Channel Select", 0, 127, 0}
            }}
        },
        {
            16, "FX", "FX-DYNAMIX", true,
            {{
                {"ATK", "Compressor Attack", 0, 127, 16}, {"REL", "Compressor Release", 0, 127, 48},
                {"THRS", "Compression Threshold", 0, 127, 64}, {"MIX", "Parallel Dry/Wet Mix", 0, 127, 127},
                {"RAT", "Compression Ratio", 0, 127, 64}, {"GAIN", "Makeup Gain", 0, 127, 64},
                {"RMS", "Peak vs RMS Detection", 0, 127, 0}, {"INP", "Input Channel Select", 0, 127, 0}
            }}
        },
        {
            17, "FX", "FX-RINGMOD", true,
            {{
                {"WAVE", "Internal Carrier Shape", 0, 127, 0}, {"EXT", "External Ring Carrier", 0, 127, 0},
                {"---", "Unused", 0, 0, 0}, {"MIX", "Wet/Dry Balance", 0, 127, 127},
                {"---", "Unused", 0, 0, 0}, {"---", "Unused", 0, 0, 0},
                {"---", "Unused", 0, 0, 0}, {"INP", "Input Channel Select", 0, 127, 0}
            }}
        },
        {
            18, "FX", "FX-PHASER", true,
            {{
                {"CNTR", "Center Frequency", 0, 127, 64}, {"DEP", "LFO Depth", 0, 127, 64},
                {"SPD", "LFO Modulation Speed", 0, 127, 48}, {"MIX", "Wet/Dry Balance", 0, 127, 64},
                {"---", "Unused", 0, 0, 0}, {"WID", "Stereo Phase Spread", 0, 127, 64},
                {"---", "Unused", 0, 0, 0}, {"INP", "Input Channel Select", 0, 127, 0}
            }}
        },
        {
            19, "FX", "FX-FLANGER", true,
            {{
                {"DEL", "Base Comb Delay", 0, 127, 20}, {"DEP", "Sweep Depth", 0, 127, 64},
                {"SPD", "LFO Speed", 0, 127, 40}, {"MIX", "Wet/Dry Balance", 0, 127, 64},
                {"---", "Unused", 0, 0, 0}, {"WID", "Stereo Width Spread", 0, 127, 64},
                {"---", "Unused", 0, 0, 0}, {"INP", "Input Channel Select", 0, 127, 0}
            }}
        }
    };
    return kMachines;
}

} // namespace monomachine
