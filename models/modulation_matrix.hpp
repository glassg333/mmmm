#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <array>
#include <algorithm>
#include <cstdint>

namespace monomachine {

/**
 * @brief Elektron Monomachine Modulation Matrix & Expression Engine.
 * 
 * Hardware Architecture & Memory Locations (OS 1.32B DSP56300 Core):
 * ------------------------------------------------------------------
 * In the Monomachine architecture, modulation values are summed into target parameter
 * accumulators in DSP X/Y memory during the pre-processing slice (between P:$000100 and P:$0002eb).
 * 
 * Sources:
 * 1. Key Tracking (Pitch / Note Number 0..127):
 *    - Base filter tracking offset (KIT > ASSIGN > KEY) added directly to Filter Base (Y:$0510)
 *      and Filter Width (Y:$0511) before coefficient calculation at P:$0005a2.
 * 2. Velocity (0..127):
 *    - Velocity scale vector applied to Amp Vol (Y:$050d), Filter Base/Width, or any Synth param.
 * 3. Free Assignable Joystick / MIDI CC Controllers:
 *    - Parameter Joy X (Pitch bend / Left-Right joystick)
 *    - Parameter Joy Y (Modulation Wheel / Up-Down joystick)
 * 
 * Target Destination IDs (0..31 matching Voice Page offsets at Y:$0500..Y:$051f):
 * - 0..7:   SYNTH_A .. SYNTH_H
 * - 8..15:  AMP_ATK .. AMP_PORT (8:ATK, 9:HOLD, 10:DEC, 11:REL, 12:DIST, 13:VOL, 14:PAN, 15:PORT)
 * - 16..23: FLTR_BASE .. FLTR_WOFS (16:BASE, 17:WDTH, 18:HPQ, 19:LPQ, 20:ATK, 21:DEC, 22:BOFS, 23:WOFS)
 * - 24..31: FX_EQF .. FX_DWID (24:EQF, 25:EQG, 26:SRR, 27:DTIM, 28:DSND, 29:DFB, 30:DBAS, 31:DWID)
 */

enum class ModSource : uint8_t {
    KeyTracking,    // MIDI Note number centered around C4 (60)
    Velocity,       // Note velocity (0..127)
    ParamX,         // Free assignable Controller X (e.g. Mod Wheel / Pitch / Macro X)
    ParamY,         // Free assignable Controller Y (e.g. Aftertouch / Breath / Macro Y)
    LFO1,           // LFO 1 Output (-1.0 .. +1.0)
    LFO2,           // LFO 2 Output (-1.0 .. +1.0)
    LFO3            // LFO 3 Output (-1.0 .. +1.0)
};

struct ModRouting {
    bool enabled = false;
    ModSource source = ModSource::Velocity;
    uint8_t targetParamId = 16; // Default to Filter Base
    int8_t depth = 0;           // Bipolar depth (-64 .. +63)
};

class ModulationMatrix {
public:
    static constexpr size_t kMaxRoutings = 8;
    static constexpr size_t kTargetCount = 32;

    ModulationMatrix() {
        m_routings.resize(kMaxRoutings);
        // Default assignment 1: Key Tracking -> Filter Base
        m_routings[0] = { true, ModSource::KeyTracking, 16, 32 };
        // Default assignment 2: Velocity -> Amp Volume
        m_routings[1] = { true, ModSource::Velocity, 13, 32 };
        // Default assignment 3: Param X -> Filter Width
        m_routings[2] = { true, ModSource::ParamX, 17, 30 };
        // Default assignment 4: Param Y -> Modulator 1 Index (SYNTH_C)
        m_routings[3] = { true, ModSource::ParamY, 2, 40 };
    }

    void reset() {
        m_keyNote = 60;
        m_velocity = 127;
        m_paramX = 64; // Centered
        m_paramY = 0;
        m_lfoOutputs.fill(0.0f);
    }

    // Set input source values
    void setNoteAndVelocity(uint8_t note, uint8_t velocity) noexcept {
        m_keyNote = note;
        m_velocity = velocity;
    }

    void setParamX(uint8_t x) noexcept {
        m_paramX = x;
    }

    void setParamY(uint8_t y) noexcept {
        m_paramY = y;
    }

    void setLfoOutputs(float lfo1, float lfo2, float lfo3) noexcept {
        m_lfoOutputs[0] = lfo1;
        m_lfoOutputs[1] = lfo2;
        m_lfoOutputs[2] = lfo3;
    }

    void configureRouting(size_t slot, bool enabled, ModSource src, uint8_t targetId, int8_t depth) {
        if (slot < kMaxRoutings) {
            m_routings[slot] = { enabled, src, targetId, depth };
        }
    }

    /**
     * @brief Evaluates all matrix modulations and computes the modulated 0..127 parameter values.
     * @param baseParams Raw voice page parameters [0..31]
     * @param modulatedParams Destination array receiving the summed, clamped [0..127] parameters
     */
    void evaluate(const std::array<uint8_t, kTargetCount>& baseParams,
                  std::array<uint8_t, kTargetCount>& modulatedParams) const noexcept {
        // Start from base parameters
        for (size_t i = 0; i < kTargetCount; ++i) {
            modulatedParams[i] = baseParams[i];
        }

        // Normalized source signals (-1.0f .. +1.0f or 0.0f .. 1.0f)
        float keyTrackNorm = (static_cast<float>(m_keyNote) - 60.0f) / 60.0f; // Centered at 60
        float velNorm      = static_cast<float>(m_velocity) / 127.0f;
        float paramXNorm   = (static_cast<float>(m_paramX) - 64.0f) / 64.0f; // Bipolar (-1 .. +1)
        float paramYNorm   = static_cast<float>(m_paramY) / 127.0f;          // Unipolar (0 .. 1)

        for (const auto& r : m_routings) {
            if (!r.enabled || r.targetParamId >= kTargetCount || r.depth == 0) continue;

            float srcVal = 0.0f;
            switch (r.source) {
                case ModSource::KeyTracking: srcVal = keyTrackNorm; break;
                case ModSource::Velocity:    srcVal = velNorm;      break;
                case ModSource::ParamX:      srcVal = paramXNorm;   break;
                case ModSource::ParamY:      srcVal = paramYNorm;   break;
                case ModSource::LFO1:        srcVal = m_lfoOutputs[0]; break;
                case ModSource::LFO2:        srcVal = m_lfoOutputs[1]; break;
                case ModSource::LFO3:        srcVal = m_lfoOutputs[2]; break;
            }

            // Depth scaling (-64 .. +63 -> +/- 64 parameter steps)
            float delta = srcVal * static_cast<float>(r.depth);
            int newVal = static_cast<int>(modulatedParams[r.targetParamId]) + static_cast<int>(std::round(delta));
            modulatedParams[r.targetParamId] = static_cast<uint8_t>(std::clamp(newVal, 0, 127));
        }
    }

    const std::vector<ModRouting>& getRoutings() const noexcept { return m_routings; }

    static std::string getDestinationName(uint8_t paramId) {
        if (paramId < 8) {
            return "SYNTH_" + std::string(1, 'A' + paramId);
        } else if (paramId < 16) {
            const char* names[] = {"AMP_ATK", "AMP_HOLD", "AMP_DEC", "AMP_REL", "AMP_DIST", "AMP_VOL", "AMP_PAN", "AMP_PORT"};
            return names[paramId - 8];
        } else if (paramId < 24) {
            const char* names[] = {"FLTR_BASE", "FLTR_WDTH", "FLTR_HPQ", "FLTR_LPQ", "FLTR_ATK", "FLTR_DEC", "FLTR_BOFS", "FLTR_WOFS"};
            return names[paramId - 16];
        } else {
            const char* names[] = {"FX_EQF", "FX_EQG", "FX_SRR", "FX_DTIM", "FX_DSND", "FX_DFB", "FX_DBAS", "FX_DWID"};
            return names[paramId - 24];
        }
    }

private:
    std::vector<ModRouting> m_routings;
    uint8_t m_keyNote = 60;
    uint8_t m_velocity = 127;
    uint8_t m_paramX = 64;
    uint8_t m_paramY = 0;
    std::array<float, 3> m_lfoOutputs{0.0f, 0.0f, 0.0f};
};

} // namespace monomachine
