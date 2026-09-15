#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <cstdint>

namespace monomachine {

/**
 * @brief Elektron Monomachine Hardware Arpeggiator (OS 1.32B Specification).
 * 
 * Hardware Parameters:
 * 1. MODE:
 *    - OFF: Arpeggiator disabled.
 *    - KEY: Standard arpeggio over keys currently held.
 *    - SID: SidStation style: pressed keys remain in loop even after release;
 *           when only 1 key is held, arp does NOT activate (plays normal monophonic note).
 *    - ADD: Keys pressed are continuously accumulated into cycle until all released.
 * 
 * 2. PLAY (Note Sorting / Direction):
 *    - TRUE: Plays in the order keys were originally played.
 *    - UP: Lowest note to highest note ascending.
 *    - DOWN: Highest to lowest descending.
 *    - CYCL: Ping-pong cycling up then down.
 *    - RND: Random selection of notes from buffer.
 * 
 * 3. SPD (Speed):
 *    - Arp clock divider (1..96, typically 1..16 sequencer sub-steps e.g. 1/16th, 1/32nd notes).
 * 
 * 4. RNGE (Octave Range):
 *    - 1 OCT, 2 OCT, 3 OCT, 4 OCT.
 * 
 * 5. LEN (Gate Length):
 *    - Gate time of each arpeggiated note (0..127 or 1..16 steps).
 */
class MonomachineArpeggiator {
public:
    enum class Mode : uint8_t { Off, Key, Sid, Add };
    enum class Play : uint8_t { True, Up, Down, Cycl, Rnd };

    struct ArpSettings {
        Mode mode = Mode::Off;
        Play play = Play::Up;
        uint8_t speed = 6;      // Sub-step ticks per note (e.g. 6 = 16th notes at 24 PPQN)
        uint8_t range = 1;      // 1..4 octaves
        uint8_t noteLength = 64;// Gate duration
    };

    struct ArpNoteEvent {
        bool triggered = false;
        uint8_t note = 60;
        uint8_t velocity = 127;
        bool isNoteOff = false;
    };

    MonomachineArpeggiator() {
        m_rng.seed(42);
    }

    void reset(double sampleRate = 44100.0, double bpm = 120.0) {
        m_sampleRate = sampleRate;
        m_bpm = bpm;
        m_heldKeys.clear();
        m_sidKeys.clear();
        m_tickCounter = 0;
        m_stepIndex = 0;
        m_currentOctave = 0;
        m_cycleDirectionUp = true;
    }

    void setTempo(double bpm) {
        m_bpm = std::clamp(bpm, 30.0, 300.0);
    }

    void setSettings(const ArpSettings& settings) {
        m_settings = settings;
    }

    void noteOn(uint8_t note, uint8_t velocity = 127) {
        // Add to held keys
        if (std::find(m_heldKeys.begin(), m_heldKeys.end(), note) == m_heldKeys.end()) {
            m_heldKeys.push_back(note);
        }

        // Add to SID persistent keys
        if (std::find(m_sidKeys.begin(), m_sidKeys.end(), note) == m_sidKeys.end()) {
            m_sidKeys.push_back(note);
        }
        m_lastVelocity = velocity;
    }

    void noteOff(uint8_t note) {
        auto it = std::find(m_heldKeys.begin(), m_heldKeys.end(), note);
        if (it != m_heldKeys.end()) {
            m_heldKeys.erase(it);
        }

        // If ADD mode and all keys released, clear accumulated keys
        if (m_settings.mode == Mode::Add && m_heldKeys.empty()) {
            m_sidKeys.clear();
        }
    }

    /**
     * @brief Process audio block sample interval and return if a note trig occurred.
     */
    ArpNoteEvent processBlock(size_t numSamples) {
        ArpNoteEvent ev;
        if (m_settings.mode == Mode::Off) return ev;

        std::vector<uint8_t> activePool;
        if (m_settings.mode == Mode::Sid) {
            // SID mode: when only 1 key is in pool, arp does not cycle
            if (m_sidKeys.size() <= 1 && m_heldKeys.size() <= 1) {
                return ev;
            }
            activePool = m_sidKeys;
        } else {
            if (m_heldKeys.empty()) return ev;
            activePool = m_heldKeys;
        }

        if (activePool.empty()) return ev;

        // Sort pool based on PLAY setting
        if (m_settings.play == Play::Up) {
            std::sort(activePool.begin(), activePool.end());
        } else if (m_settings.play == Play::Down) {
            std::sort(activePool.begin(), activePool.end(), std::greater<uint8_t>());
        }

        // Calculate samples per arp tick (24 PPQN standard)
        double samplesPerBeat = (m_sampleRate * 60.0) / m_bpm;
        double samplesPerTick = (samplesPerBeat / 24.0) * std::max<uint8_t>(1, m_settings.speed);

        m_tickCounter += numSamples;
        if (m_tickCounter >= samplesPerTick) {
            m_tickCounter -= samplesPerTick;

            // Pick note
            uint8_t rawNote = 60;
            if (m_settings.play == Play::Rnd) {
                std::uniform_int_distribution<size_t> dist(0, activePool.size() - 1);
                rawNote = activePool[dist(m_rng)];
                std::uniform_int_distribution<int> octDist(0, m_settings.range - 1);
                m_currentOctave = octDist(m_rng);
            } else if (m_settings.play == Play::Cycl) {
                if (m_stepIndex >= activePool.size()) {
                    m_stepIndex = 0;
                    m_cycleDirectionUp = !m_cycleDirectionUp;
                    if (m_cycleDirectionUp) {
                        m_currentOctave = (m_currentOctave + 1) % std::max<uint8_t>(1, m_settings.range);
                    }
                }
                size_t idx = m_cycleDirectionUp ? m_stepIndex : (activePool.size() - 1 - m_stepIndex);
                rawNote = activePool[idx % activePool.size()];
                ++m_stepIndex;
            } else {
                if (m_stepIndex >= activePool.size()) {
                    m_stepIndex = 0;
                    m_currentOctave = (m_currentOctave + 1) % std::max<uint8_t>(1, m_settings.range);
                }
                rawNote = activePool[m_stepIndex];
                ++m_stepIndex;
            }

            int finalNote = static_cast<int>(rawNote) + (m_currentOctave * 12);
            ev.triggered = true;
            ev.note = static_cast<uint8_t>(std::clamp(finalNote, 0, 127));
            ev.velocity = m_lastVelocity;
        }

        return ev;
    }

    static std::vector<std::string> getArpDropdownHierarchy() {
        return {
            "[MODE]",
            "  OFF",
            "  KEY",
            "  SID",
            "  ADD",
            "[PLAY]",
            "  TRUE",
            "  UP",
            "  DOWN",
            "  CYCL",
            "  RND",
            "[RANGE]",
            "  1 OCT",
            "  2 OCT",
            "  3 OCT",
            "  4 OCT",
            "[SPEED]",
            "  1 (Fastest)",
            "  3 (Triplets)",
            "  6 (16th)",
            "  12 (8th)",
            "  24 (1/4)"
        };
    }

private:
    double m_sampleRate = 44100.0;
    double m_bpm = 120.0;
    ArpSettings m_settings;

    std::vector<uint8_t> m_heldKeys;
    std::vector<uint8_t> m_sidKeys;
    uint8_t m_lastVelocity = 127;

    double m_tickCounter = 0.0;
    size_t m_stepIndex = 0;
    int m_currentOctave = 0;
    bool m_cycleDirectionUp = true;
    std::mt19937 m_rng;
};

} // namespace monomachine
