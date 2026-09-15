#pragma once

#include "../models/machine_definitions.hpp"
#include "../models/track_pages.hpp"
#include "../models/arpeggiator.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace monomachine {

/**
 * @brief High-contrast Black & White Theme definitions matching gui.jpg.
 */
struct GuiTheme {
    uint32_t background = 0xFF000000;    // Pure Black
    uint32_t panelFill  = 0xFF000000;    // Pure Black
    uint32_t panelBorder= 0xFFFFFFFF;    // Pure White (dotted/solid lines)
    uint32_t textPrimary= 0xFFFFFFFF;    // Pure White
    uint32_t textSecondary= 0xFFCCCCCC;  // Off White
    uint32_t knobArc    = 0xFFFFFFFF;    // White knob stroke
    uint32_t faderFill  = 0xFFFFFFFF;    // White meter & slider indicator
};

struct KnobWidgetDesc {
    int pageIndex;
    int knobIndex;
    std::string id;
    std::string label;        // Short header e.g. "1FRQ", "BASE"
    std::string fullTitle;
    uint8_t minVal = 0;
    uint8_t maxVal = 127;
    uint8_t currentVal = 0;
    bool isBipolar = false;
    bool isNumericDisplay = false; // e.g. 1.0, 1.33 for FM ratios
    float displayRatio = 1.0f;
};

struct SectionWidgetDesc {
    std::string sectionTitle;
    int gridColumn; // 0, 1, 2
    int gridRow;    // 0 (Top), 1 (Bottom)
    std::array<KnobWidgetDesc, 8> knobs;
};

class MonomachineGuiLayout {
public:
    static GuiTheme getTheme() {
        return GuiTheme{};
    }

    static std::vector<std::string> getMachineDropdownMenuHierarchy() {
        std::vector<std::string> menu;
        std::string currentCategory = "";
        for (const auto& mach : getAllMachineDefinitions()) {
            if (mach.category != currentCategory) {
                currentCategory = mach.category;
                menu.push_back("[" + currentCategory + "]");
            }
            menu.push_back("  " + mach.name);
        }
        return menu;
    }

    static std::vector<std::string> getArpDropdownHierarchy() {
        return MonomachineArpeggiator::getArpDropdownHierarchy();
    }

    static std::array<SectionWidgetDesc, 6> createFullGridLayout(uint8_t selectedMachineId) {
        std::array<SectionWidgetDesc, 6> grid;

        // 1. SYNTHESIS SECTION (Machine A..H)
        grid[0].sectionTitle = "SYNTHESIS";
        grid[0].gridColumn = 0;
        grid[0].gridRow = 0;
        const auto& machines = getAllMachineDefinitions();
        const MachineDef* activeMach = nullptr;
        for (const auto& m : machines) {
            if (m.id == selectedMachineId) {
                activeMach = &m;
                break;
            }
        }
        if (!activeMach) activeMach = &machines[13]; // Default to FM+ DYN (id 10)

        for (int i = 0; i < 8; ++i) {
            grid[0].knobs[i].pageIndex = 0;
            grid[0].knobs[i].knobIndex = i;
            grid[0].knobs[i].id = "synth_" + std::to_string(i);
            grid[0].knobs[i].label = activeMach->synthParams[i].name;
            grid[0].knobs[i].fullTitle = activeMach->synthParams[i].fullName;
            grid[0].knobs[i].minVal = activeMach->synthParams[i].minVal;
            grid[0].knobs[i].maxVal = activeMach->synthParams[i].maxVal;
            grid[0].knobs[i].currentVal = activeMach->synthParams[i].defaultVal;
        }

        // 2. AMPLIFICATION SECTION
        grid[1].sectionTitle = "AMPLIFICATION";
        grid[1].gridColumn = 1;
        grid[1].gridRow = 0;
        auto ampDefs = getAmpPageDefinition();
        for (int i = 0; i < 8; ++i) {
            grid[1].knobs[i].pageIndex = 1;
            grid[1].knobs[i].knobIndex = i;
            grid[1].knobs[i].id = "amp_" + std::to_string(i);
            grid[1].knobs[i].label = ampDefs[i].name;
            grid[1].knobs[i].fullTitle = ampDefs[i].label;
            grid[1].knobs[i].currentVal = ampDefs[i].defaultVal;
            grid[1].knobs[i].isBipolar = ampDefs[i].isBipolar;
        }

        // 3. LFO 1 SECTION
        grid[2].sectionTitle = "LFO1";
        grid[2].gridColumn = 2;
        grid[2].gridRow = 0;
        auto lfo1Defs = getLfoPageDefinition(1);
        for (int i = 0; i < 8; ++i) {
            grid[2].knobs[i].pageIndex = 4;
            grid[2].knobs[i].knobIndex = i;
            grid[2].knobs[i].id = "lfo1_" + std::to_string(i);
            grid[2].knobs[i].label = lfo1Defs[i].name;
            grid[2].knobs[i].fullTitle = lfo1Defs[i].label;
            grid[2].knobs[i].currentVal = lfo1Defs[i].defaultVal;
            grid[2].knobs[i].isBipolar = lfo1Defs[i].isBipolar;
        }

        // 4. FILTER SECTION
        grid[3].sectionTitle = "FILTER";
        grid[3].gridColumn = 0;
        grid[3].gridRow = 1;
        auto fltDefs = getFilterPageDefinition();
        for (int i = 0; i < 8; ++i) {
            grid[3].knobs[i].pageIndex = 2;
            grid[3].knobs[i].knobIndex = i;
            grid[3].knobs[i].id = "filter_" + std::to_string(i);
            grid[3].knobs[i].label = fltDefs[i].name;
            grid[3].knobs[i].fullTitle = fltDefs[i].label;
            grid[3].knobs[i].currentVal = fltDefs[i].defaultVal;
            grid[3].knobs[i].isBipolar = fltDefs[i].isBipolar;
        }

        // 5. EFFECTS SECTION
        grid[4].sectionTitle = "EFFECTS";
        grid[4].gridColumn = 1;
        grid[4].gridRow = 1;
        auto fxDefs = getEffectsPageDefinition();
        for (int i = 0; i < 8; ++i) {
            grid[4].knobs[i].pageIndex = 3;
            grid[4].knobs[i].knobIndex = i;
            grid[4].knobs[i].id = "fx_" + std::to_string(i);
            grid[4].knobs[i].label = fxDefs[i].name;
            grid[4].knobs[i].fullTitle = fxDefs[i].label;
            grid[4].knobs[i].currentVal = fxDefs[i].defaultVal;
            grid[4].knobs[i].isBipolar = fxDefs[i].isBipolar;
        }

        // 6. LFO 2 [LFO 3] SECTION
        grid[5].sectionTitle = "LFO2 [LFO3]";
        grid[5].gridColumn = 2;
        grid[5].gridRow = 1;
        auto lfo2Defs = getLfoPageDefinition(2);
        for (int i = 0; i < 8; ++i) {
            grid[5].knobs[i].pageIndex = 5;
            grid[5].knobs[i].knobIndex = i;
            grid[5].knobs[i].id = "lfo2_" + std::to_string(i);
            grid[5].knobs[i].label = lfo2Defs[i].name;
            grid[5].knobs[i].fullTitle = lfo2Defs[i].label;
            grid[5].knobs[i].currentVal = lfo2Defs[i].defaultVal;
            grid[5].knobs[i].isBipolar = lfo2Defs[i].isBipolar;
        }

        return grid;
    }
};

} // namespace monomachine
