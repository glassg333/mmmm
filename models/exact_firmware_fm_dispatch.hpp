#pragma once

#include <cstdint>
#include <string>

namespace monomachine {

/**
 * ============================================================================
 * ELEKTRON MONOMACHINE OS 1.32B DSP56300 DISASSEMBLY & MEMORY ARCHITECTURE
 * ============================================================================
 * 
 * Machine 10: FM+ DYNAMIC (FM-Dyn)
 * 
 * 1. DISPATCH VECTORS (DSP Program Memory P-Space):
 *    - Jump Table Base (Init):    P:$10016b + 10 = P:$100175
 *    - Jump Table Base (Config):  P:$10018d + 10 = P:$100197
 *    - Jump Table Base (Process): P:$1001af + 10 = P:$1001b9
 * 
 * 2. FIRMWARE VOICE PAGE MAPPING (Y-Memory Base Y:$0500):
 *    The ColdFire (MC68k architecture) host processor converts user knob parameters
 *    into 24-bit fixed-point DSP words (left-shifted 16 bits for standard 0..127 range):
 * 
 *    - Y:$0500: SYNTH_A (1FRQ) - Modulator 1 linear frequency multiplier
 *    - Y:$0501: SYNTH_B (1FEN) - Modulator 1 frequency envelope depth & rate
 *    - Y:$0502: SYNTH_C (1VOL) - Modulator 1 volume / FM modulation index
 *    - Y:$0503: SYNTH_D (1VEN) - Modulator 1 volume envelope depth
 *    - Y:$0504: SYNTH_E (2FRQ) - Modulator 2 exponential frequency multiplier
 *    - Y:$0505: SYNTH_F (2ENV) - Modulator 2 volume & envelope depth
 *    - Y:$0506: SYNTH_G (2FB)  - Modulator 2 recursive phase feedback loop
 *    - Y:$0507: SYNTH_H (TUNE) - Master pitch fine tune
 * 
 * 3. HARDWARE PITCH & OCTAVE CALCULATION:
 *    The Monomachine scheduler computes the DSP pitch word for address Y:$0529
 *    using an 11-bit octave fraction (0x800 per octave):
 * 
 *        dspPitchWord = 0x5800 + ((midiNote * 0x800 + 6) / 12)
 * 
 *    At the machine process boundary (P:$1001b9), Register A contains:
 *        A = dspPitchWord + ((TUNE - 64) * tuneStep)
 * 
 * 4. FM+ DYN DISASSEMBLY ROUTINE OUTLINE (DSP56300 Core Execution):
 *    - R0: Pointer to audio input block (or internal buffer)
 *    - R6 / R7: State buffer in X/Y data memory holding oscillator phase accumulators:
 *        X:(R6)+0: Phase accumulator Carrier (24-bit phase)
 *        X:(R6)+1: Phase accumulator Modulator 1
 *        X:(R6)+2: Phase accumulator Modulator 2
 *        X:(R6)+3: Modulator 2 feedback history sample
 *        Y:(R7)+0: Envelope 1 state
 *        Y:(R7)+1: Envelope 2 state
 * 
 *    Assembly processing block loop (16 frames per block):
 *    -----------------------------------------------------
 *    DO #16, _end_fm_dyn_block
 *        ; 1. Modulator 2 computation (exponential ratio + feedback)
 *        MOVE    X:(R6+2), X0            ; Load Mod 2 phase
 *        MOVE    X:(R6+3), Y0            ; Load Mod 2 feedback history
 *        MAC     Y0, X1, A               ; A = Phase + Feedback * FB_gain
 *        JSR     <dsp56k_sin_lookup>     ; Fast 24-bit wavetable sine lookup
 *        MOVE    A, X:(R6+3)             ; Store new feedback sample
 *        
 *        ; 2. Modulator 1 computation (linear ratio + Mod 2 modulation)
 *        MOVE    X:(R6+1), X0            ; Load Mod 1 phase
 *        MAC     A, Y1, X0               ; Modulate Mod 1 with Mod 2 output
 *        JSR     <dsp56k_sin_lookup>     ; Calculate Mod 1 sine
 * 
 *        ; 3. Carrier computation
 *        MOVE    X:(R6+0), X0            ; Load Carrier phase
 *        MAC     A, Y0, X0               ; Modulate Carrier with Mod 1 output
 *        JSR     <dsp56k_sin_lookup>     ; Calculate Carrier sine
 * 
 *        ; 4. Output store & phase advance
 *        MOVE    A, Y:(R0)+              ; Write to 16-sample track vector
 *    _end_fm_dyn_block:
 *        RTS
 * ============================================================================
 */

} // namespace monomachine
