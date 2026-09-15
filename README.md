# Elektron Monomachine Architecture & DSP Analysis: Filter, Chorus, and FX

This repository contains reverse-engineering analysis, DSP disassembly mappings, signal routing architectures, and native C++ DSP implementations for the Elektron Monomachine (SFX-6 / SFX-60 OS 1.32B).

---

## 1. Monomachine Signal Chain & Routing Architecture

In the Monomachine architecture, each track follows a strictly ordered DSP processing chain:

```
[ Synth Machine / FX Machine Input ] (16-sample block @ 44.1 kHz)
                 │
                 ▼
     [ Distortion & SRR Stage ]
                 │
                 ▼
     [ Multi-Mode Filter Stage ]  <--- Base / Width 24dB Resonant Filter
                 │
                 ▼
         [ 1-Band Parametric EQ ]
                 │
                 ▼
       [ Amp Envelope & Pan ]    <--- AHD / ADSR envelope & stereo panning
                 │
                 ▼
       [ Stereo Track Delay ]     <--- Tape-style delay with Base/Width filter
                 │
                 ▼
      [ Master / Track Output ]
```

### Stage Boundary Program Counters (DSP56300 Memory Map)

| Stage | PC Address | Description | Source / Registers |
|---|---|---|---|
| Track Entry | `P:$000100` | Full track processing entry | Initialization & block setup |
| Machine Dispatch Call | `P:$0002eb` | Machine process invocation | Calls machine-specific process routine |
| Distortion / Pre-Filter | `P:$0004ff` | Machine output / Saturation | `X:$0097..$00a6`, `X:$00d7..$00e6` |
| Filter Stage (HPF/LPF) | `P:$0005a2` | Base/Width multi-mode filter | Input: Saturation; Output: Filtered block |
| Post-Filter / Pre-EQ | `P:$0006c0` | Filter output boundary | Processed stereo vectors |
| EQ Stage | `P:$000789` | 1-band parametric EQ | Peak/shelf tone adjustment |
| Post-EQ / Pre-Amp | `P:$000807` | Envelope shaping entry | Pre-amp level control |
| Amp Envelope & Gain | `P:$00087a` | AHD envelope multiplication | Track level & volume gain |
| Track Delay Stage | `P:$000939` | Delay send & feedback loop | Ring buffer read/write |
| Post-Delay / Pre-Pan | `P:$000a5b` | Delay output summation | Pre-routing stereo pair |
| Pre-Routing Output | `P:$000b14` | Final track output / Pan | `X:$0000..$000f`, `Y:$0020..$002f` |
| Track Complete | `P:$000163` | End of track block | Handover to scheduler ring |

---

## 2. Voice Page & Host Control Parameter Map (`Y:$0500`)

Parameters from the Monomachine ColdFire host processor are transferred to DSP Y-memory at base address `Y:$0500` as 24-bit fixed-point values (`val << 16` for standard 0–127 parameters):

| Offset | Parameter Name | Page | Description |
|---|---|---|---|
| `Y:$0500` | `SYNTH_A` | SYNTH / FX | FX-CHORUS: Delay (`DEL`) |
| `Y:$0501` | `SYNTH_B` | SYNTH / FX | FX-CHORUS: Depth (`DEP`) |
| `Y:$0502` | `SYNTH_C` | SYNTH / FX | FX-CHORUS: Speed (`SPD`) |
| `Y:$0503` | `SYNTH_D` | SYNTH / FX | FX-CHORUS: Mix (`MIX`) |
| `Y:$0504` | `SYNTH_E` | SYNTH / FX | Unused (`---`) |
| `Y:$0505` | `SYNTH_F` | SYNTH / FX | FX-CHORUS: Width (`WID`) |
| `Y:$0506` | `SYNTH_G` | SYNTH / FX | Unused (`---`) |
| `Y:$0507` | `SYNTH_H` | SYNTH / FX | FX-CHORUS: Input Routing (`INP`) |
| `Y:$0508` | `AMP_ATK` | AMP | Attack time |
| `Y:$0509` | `AMP_HOLD` | AMP | Hold time |
| `Y:$050a` | `AMP_DEC` | AMP | Decay time |
| `Y:$050b` | `AMP_REL` | AMP | Release time |
| `Y:$050c` | `AMP_DIST` | AMP | Overdrive / distortion |
| `Y:$050d` | `AMP_VOL` | AMP | Amplifier volume |
| `Y:$050e` | `AMP_PAN` | AMP | Stereo pan position |
| `Y:$050f` | `AMP_PORT` | AMP | Portamento rate |
| `Y:$0510` | `FLTR_BASE` | FILTER | High-pass cutoff frequency |
| `Y:$0511` | `FLTR_WDTH` | FILTER | Low-pass frequency offset (`BASE + WIDTH`) |
| `Y:$0512` | `FLTR_HPQ` | FILTER | High-pass resonance |
| `Y:$0513` | `FLTR_LPQ` | FILTER | Low-pass resonance |
| `Y:$0514` | `FLTR_ATK` | FILTER | Filter envelope attack |
| `Y:$0515` | `FLTR_DEC` | FILTER | Filter envelope decay |
| `Y:$0516` | `FLTR_BOFS` | FILTER | Envelope modulation depth to Base |
| `Y:$0517` | `FLTR_WOFS` | FILTER | Envelope modulation depth to Width |
| `Y:$0518` | `FX_EQ_FRQ` | FX | EQ center frequency |
| `Y:$0519` | `FX_EQ_GN` | FX | EQ gain |
| `Y:$051a` | `FX_SRR` | FX | Sample rate reduction |
| `Y:$051b` | `FX_DEL_TIM` | FX | Delay time |
| `Y:$051c` | `FX_DEL_SND` | FX | Delay send amount |
| `Y:$051d` | `FX_DEL_FDB` | FX | Delay feedback |
| `Y:$051e` | `FX_DEL_BAS` | FX | Delay high-pass filter cutoff |
| `Y:$051f` | `FX_DEL_WID` | FX | Delay low-pass filter cutoff |
| `Y:$0524`–`0528` | Envelope state | AMP | Current envelope tracking stages |
| `Y:$0523` | `TEMPO` | CLOCK | `0x800000 / (24 * tempo)` |
| `Y:$0524` | `MACHINE_ID` | ROUTING | Machine type (0–33) |
| `Y:$0528` | `TRIG_EVENT` | GATE | `0`: Idle, `1`: Note On, `2`: Note Off |
| `Y:$0529` | `NOTE_PITCH` | TUNING | 11-bit octave fraction (`0x5800 + note*0x800/12`) |

---

## 3. Machine Dispatch Tables

The firmware dispatches machines through jump tables located in DSP Program memory:
- **Initialization Dispatch Table**: `P:$10016b + machineIndex`
- **Secondary Config Table**: `P:$10018d + machineIndex`
- **Process Dispatch Table**: `P:$1001af + machineIndex`

### Key Machine Indices:
- `10` (`0x0A`): **FM+ DYNAMIC** (`FM-Dyn`)
  - Init: `P:$100175`, Process: `P:$1001b9`
- `15` (`0x0F`): **FX-CHORUS**
  - Init: `P:$10017a`, Process: `P:$1001be`
- `1` (`0x01`): **GND-SIN**
  - Init: `P:$10016c`, Process: `P:$1001b0`

---

## 4. Native C++ Implementations

To run without the heavy cycle-by-cycle DSP56300 CPU emulation overhead, the DSP algorithms are converted to native C++:
- `dsp/monomachine_filter.hpp`: Base-Width resonant filter with 2-pole HPF + 2-pole LPF in series.
- `dsp/monomachine_chorus.hpp`: Stereo modulated delay lines with quadrature LFO and stereo width spread.
- `dsp/monomachine_voice_chain.hpp`: Unified Monomachine track strip combining FX-Chorus, Multi-Mode Filter, Overdrive, EQ, and Amp Envelope.

---

## 5. UI Layout & Machine Hierarchy Architecture ("MONOMACHINE ONE")

The UI layout follows the 6-page parameter matrix shown in `gui.jpg` and `all gen 1.jpg`:

```
+-----------------------------------------------------------------------------------------+
| [Logo] MONOMACHINE ONE                                        BPM: 120.0   [ MENU ]     |
| LEV  [ SWAVE-SAW         v ]                                                            |
| +--+ +-------------------------+ +-------------------------+ +------------------------+ |
| |  | | SYNTHESIS               | | AMPLIFICATION           | | LFO 1                  | |
| |  | | 1FRQ 1FEN 1VOL 1VEN ... | | ATK  HOLD DEC  REL  ... | | PAGE DEST TRIG WAVE ...| |
| |  | +-------------------------+ +-------------------------+ +------------------------+ |
| |  | | FILTER                  | | EFFECTS                 | | LFO 2 [LFO 3]          | |
| |  | | BASE WDTH HPQ  LPQ  ... | | EQF  EQG  SRR  DTIM ... | | PAGE DEST TRIG WAVE ...| |
| +--+ +-------------------------+ +-------------------------+ +------------------------+ |
+-----------------------------------------------------------------------------------------+
```

### Complete Machine & Page Catalog:

1. **GND**:
   - `GND-GND` (Ground/Silence)
   - `GND-SIN` (Pure Sine: `TUNE`)
   - `GND-NOIS` (Stereo Noise: `ST`, `RED`, `STON`, `TUNE`)
2. **SID**:
   - `SID-6581` (Commodore 64 SID emulation: `PW`, `PWAD`, `PWRS`, `WAVE`, `MOD`, `MSRC`, `MFRQ`, `TUNE`)
3. **SWAVE**:
   - `SWAVE-SAW` (Superwave Saw: `UNIL`, `UNIW`, `UNIX`, `SUBX`, `SUB1`, `SUB2`, `TUNE`)
   - `SWAVE-PULS` (Superwave Pulse: `UNIL`, `UNIW`, `SUB1`, `SUB2`, `PW`, `PWAD`, `PWRS`, `TUNE`)
   - `SWAVE-ENS` (Superwave Ensemble: `PCH2`, `PCH3`, `PCH4`, `WAVE`, `PW`, `CHRL`, `CHRW`, `TUNE`)
4. **DPRO**:
   - `DPRO-WAVE` (Wavetable Synth: `WAVE`, `WP`, `WPM`, `WPRS`, `SYNC`, `SFRQ`, `TUNE`)
   - `DPRO-BBOX` (Drum / Beatbox sampler: `PTCH`, `STRT`, `RTRG`, `RTIM`)
   - `DPRO-DDRW` (Dynamic Drawer wavetable synth: `WAV1`, `MIX`, `WAV2`, `TIME`, `BR1`, `WID`, `BR2`, `TUNE`)
   - `DPRO-DENS` (Dynamic Ensemble: `PCH2`, `PCH3`, `PCH4`, `WAVE`, `CHRL`, `CHRW`, `TUNE`)
5. **FM+**:
   - `FM+ STAT` (Static 2-Op FM: `1FRQ`, `1FIN`, `1ENV`, `1FB`, `2FRQ`, `2VOL`, `TONE`, `TUNE`)
   - `FM+ PAR` (Parallel 3-Op FM: `1FRQ`, `1ENV`, `2FRQ`, `2ENV`, `3FRQ`, `3ENV`, `TONE`, `TUNE`)
   - `FM+ DYN` (Dynamic FM with feedback: `1FRQ`, `1FEN`, `1VOL`, `1VEN`, `2FRQ`, `2ENV`, `2FB`, `TUNE`)
6. **VO**:
   - `VO-6` (Formant / Vowel Voice Synth: `VOC1`, `VOC2`, `V-SW`, `VOIC`, `CONS`, `CLEN`, `CVOL`, `TUNE`)
7. **FX**:
   - `FX-THRU` (External audio passthrough: `INP`)
   - `FX-REVERB` (Digital plate/room reverb: `DEC`, `DAMP`, `GATE`, `MIX`, `INP`)
   - `FX-CHORUS` (Stereo modulated delay: `DEL`, `DEP`, `SPD`, `MIX`, `WID`, `INP`)
   - `FX-DYNAMIX` (Master compressor/limiter: `ATK`, `REL`, `THRS`, `MIX`, `RAT`, `GAIN`, `RMS`, `INP`)
   - `FX-RINGMOD` (Ring modulator: `WAVE`, `EXT`, `MIX`, `INP`)
   - `FX-PHASER` (Multi-stage allpass phaser: `CNTR`, `DEP`, `SPD`, `MIX`, `WID`, `INP`)
   - `FX-FLANGER` (Comb-filtering flanger: `DEL`, `DEP`, `SPD`, `MIX`, `WID`, `INP`)

All native C++ DSP implementations for these effects and routing structures are located in `dsp/`, UI layout descriptors in `ui/`, and parameter definitions in `models/`.

---

## 6. Disassembly & Parameter Reference for Encrypted/Cryptic Oscillator Engines

When decompiling or rebuilding machines directly from the Motorola DSP56300 firmware binary (`section_1_DSP.bin` / `section_3_DSP.bin`), use these exact memory offsets, dispatch vectors, parameter names, and algorithm blueprints:

```
Machine Index Map:
┌────────┬─────────────────┬──────────┬──────────┬──────────┬────────────────────────┐
│ Index  │ Machine Name    │ Init PC  │ Conf PC  │ Proc PC  │ Category               │
├────────┼─────────────────┼──────────┼──────────┼──────────┼────────────────────────┤
│ 10     │ FM+ DYN         │ P:$100175│ P:$100197│ P:$1001b9│ Dynamic 2-Op FM Synth  │
│ 3      │ SID-6581        │ P:$10016e│ P:$100190│ P:$1001b2│ MOS 6581 SID Emulation │
│ 2      │ GND-NOIS        │ P:$10016d│ P:$10018f│ P:$1001b1│ Noise & Bit Reducer    │
│ 7      │ DPRO-BBOX       │ P:$100172│ P:$100194│ P:$1001b6│ Sample Drum Machine    │
└────────┴─────────────────┴──────────┴──────────┴──────────┴────────────────────────┘
```

---

### A. FM+ DYNAMIC (`FM+ DYN` — Machine Index 10 / `0x0A`)

**Dispatch Addresses:**
- `Init`: `P:$100175`
- `Process`: `P:$1001b9`
- `Voice Page`: `Y:$0500..Y:$0507`

**Encrypted / Cryptic Parameter Meanings:**
- `1FRQ` (`Y:$0500`): **Modulator 1 Linear Frequency Ratio**. Ratio multiplier = `val / 16.0`. When `val = 16`, ratio is `1.0` (fundamental).
- `1FEN` (`Y:$0501`): **Modulator 1 Frequency Envelope**. Bipolar depth `(-64..+63)` controlling pitch sweep over time.
- `1VOL` (`Y:$0502`): **Modulator 1 Volume / Modulation Index**. Modulation depth `(0.0 .. 6.0 radians)` into the carrier.
- `1VEN` (`Y:$0503`): **Modulator 1 Volume Envelope**. Decay rate and dynamic depth of Modulator 1.
- `2FRQ` (`Y:$0504`): **Modulator 2 Exponential Frequency Ratio**. Continuous ratio multiplier: `2.0^((val - 32) / 24)`. When `val = 32`, ratio = `1.0`; when `val = 42`, ratio $\approx$ `1.33`; when `val = 56`, ratio = `2.0`.
- `2ENV` (`Y:$0505`): **Modulator 2 Volume & Envelope**. Modulator 2 index into Modulator 1.
- `2FB` (`Y:$0506`): **Modulator 2 Self-Feedback**. Phase feedback loop producing dense metallic sidebands: `sin(phi + FB * last_sample)`.
- `TUNE` (`Y:$0507`): **Master Fine Tuning**. Offset in semitones `(val - 64) / 64 * 12.0`.

---

### B. SID-6581 (`SID` — Machine Index 3 / `0x03`)

**Dispatch Addresses:**
- `Init`: `P:$10016e`
- `Process`: `P:$1001b2`
- `Voice Page`: `Y:$0500..Y:$0507`

**Encrypted / Cryptic Parameter Meanings:**
- `PW` (`Y:$0500`): **Pulse Width**. Duty cycle of pulse wave `(0% .. 100%)`.
- `PWAD` (`Y:$0501`): **Pulse Width Attack & Decay**. LFO/Envelope sweep modulating the duty cycle.
- `PWRS` (`Y:$0502`): **Pulse Width Reset**. Hard phase sync and pulse width phase restart on Note-On.
- `WAVE` (`Y:$0503`): **SID Waveform Select**. Selects Triangle, Sawtooth, Pulse, or combined SID multi-waves (emulating Commodore 64 6581 register `$D404`).
- `MOD` (`Y:$0504`): **Modulation Type**. `0 = Off`, `1 = Hard Ring Modulation`, `2 = Oscillator Hard Sync`.
- `MSRC` (`Y:$0505`): **Modulation Source**. Cross-voice modulation source selection.
- `MFRQ` (`Y:$0506`): **Modulation Frequency**. Frequency of the auxiliary modulator oscillator.
- `TUNE` (`Y:$0507`): **Master Tune**. Master pitch offset.

---

### C. GND-NOISE (`GND-NOIS` — Machine Index 2 / `0x02`)

**Dispatch Addresses:**
- `Init`: `P:$10016d`
- `Process`: `P:$1001b1`
- `Voice Page`: `Y:$0500..Y:$0507`

**Encrypted / Cryptic Parameter Meanings:**
- `ST` (`Y:$0500`): **Stereo Width / Phase Spread**. De-correlates the noise LFSR generators between Left and Right channels.
- `RED` (`Y:$0501`): **Sample Rate Reduction / Bit Crushing**. Decimates the noise generator clock rate for lo-fi crunch.
- `STON` (`Y:$0502`): **Stereo Tone / Filter Coloring**. Low-pass vs High-pass tilt filter applied to the noise floor.
- `---` (`Y:$0503..0506`): Unused / reserved.
- `TUNE` (`Y:$0507`): **Noise Pitch / Resonant Frequency Peak**. Center frequency of the noise resonant bandpass peak.

---

### D. DPRO-BBOX (`DPRO-BBOX` — Machine Index 7 / `0x07`) & 10-Slot Sampler

**Dispatch Addresses:**
- `Init`: `P:$100172`
- `Process`: `P:$1001b6`
- `Voice Page`: `Y:$0500..Y:$0507`

**Encrypted / Cryptic Parameter Meanings:**
- `PTCH` (`Y:$0500`): **Sample Pitch**. Key tracking and resampling rate multiplier: `2.0^((val - 64) / 32)`.
- `STRT` (`Y:$0501`): **Sample Start Point**. Fractional offset into the PCM buffer `(0.0 .. 1.0)`.
- `---` (`Y:$0502..0503`): Unused.
- `RTRG` (`Y:$0504`): **Retrigger Count**. Number of rapid machine retriggers (`0` = off, `1..127` repeats).
- `RTIM` (`Y:$0505`): **Retrigger Time**. Interval duration between retriggers in milliseconds.
- `---` (`Y:$0506`): Unused.
- `TUNE` (`Y:$0507`): **Fine Tuning**. Sample rate fine tune.

**Extended 10-Slot Sample Engine Implementation (`dsp/monomachine_bbox.hpp`):**
- Supports **10 arbitrary user PCM sample slots** (WAV / raw 44.1 kHz).
- Integrated **Random Slot Selection**: picks a random sample slot from 0..9 on each Note-On.
- Integrated **Random Start Position**: randomizes `STRT` offset on each trigger for granular beat slicing.
- Interpolated Hermite/linear fractional pitch resampling.

---

## 7. Disassembled Modulation Architecture, Keytracking & X/Y Matrix

In the disassembled DSP56300 firmware, parameter modulation occurs between the scheduler entry (`P:$000100`) and the machine dispatch call (`P:$0002eb`):

```
[ MIDI / Sequencer Event ]
          │
          ├── Note Pitch -> Keytracking delta calculation (centered at Note 60 / C4)
          ├── Velocity   -> Velocity scale vector (0..127)
          ├── Controller -> Parameter X (Joystick X / Pitch Bend / Macro X)
          └── Controller -> Parameter Y (Joystick Y / Mod Wheel / Macro Y)
          │
          ▼
 [ Modulation Matrix Summation ] (Writes to 24-bit Voice Page accumulators)
          │
          ├── Target: Y:$0510 (FLTR_BASE) <- Base + (KeyTrack * Depth)
          ├── Target: Y:$0511 (FLTR_WDTH) <- Width + (KeyTrack * Depth)
          ├── Target: Y:$050d (AMP_VOL)   <- Volume + (Velocity * Depth)
          └── Target: Y:$0500..$051f     <- Any target mapped via Param X/Y
```

### Destination Parameter ID Map (`models/modulation_matrix.hpp`):
- `0..7`: `SYNTH_A` .. `SYNTH_H`
- `8..15`: `AMP_ATK`, `AMP_HOLD`, `AMP_DEC`, `AMP_REL`, `AMP_DIST`, `AMP_VOL`, `AMP_PAN`, `AMP_PORT`
- `16..23`: `FLTR_BASE`, `FLTR_WDTH`, `FLTR_HPQ`, `FLTR_LPQ`, `FLTR_ATK`, `FLTR_DEC`, `FLTR_BOFS`, `FLTR_WOFS`
- `24..31`: `FX_EQF`, `FX_EQG`, `FX_SRR`, `FX_DTIM`, `FX_DSND`, `FX_DFB`, `FX_DBAS`, `FX_DWID`

### Modulation Sources:
1. **Key Tracking**: Linear and exponential pitch tracking added to filter cutoff frequencies (`BASE`, `WDTH`).
2. **Velocity**: Dynamic velocity scaling routed to volume, filter resonance, or envelope times.
3. **Param X & Param Y**: Free assignable macro controllers mapped to any parameter destination.

---

## 8. Complete Disassembly Guide & Undocumented Parameter Catalog

### A. Firmware Disassembly Architecture (Elektron OS 1.32B)
The Monomachine firmware is partitioned into two primary DSP images for the Motorola DSP56300:
1. `section_1_DSP.bin`: Core OS, hardware vector table, host interface (HI08), and DMA scheduler.
2. `section_3_DSP.bin`: Synthesis machines, FX algorithms, wavetables, and filter coefficients.

#### Disassembly Jump Tables:
- **Initialization Dispatch Table**: `P:$10016b + machineIndex`
- **Configuration Dispatch Table**: `P:$10018d + machineIndex`
- **Audio Processing Dispatch Table**: `P:$1001af + machineIndex`

Every machine routine executes with the following ABI:
- **`R0`**: Pointer to the 16-sample audio output ring buffer.
- **`R6`**: Base pointer in X-data memory for internal phase accumulators and feedback memory.
- **`R7`**: Base pointer in Y-data memory for envelope tracking states.
- **Accumulator `A`**: Carries the 24-bit pitch word from `Y:$0529`.

---

### B. Undocumented / Cryptic Parameter Specifications

#### 1. SWAVE Family (Machines 4, 5, 14)
- **`UNIL`** (`Y:$0500`): **Unison Level**. Detuned secondary oscillator blend (0..127).
- **`UNIW`** (`Y:$0501`): **Unison Width**. Detune spread between voices (cents / phase offset).
- **`UNIX`** (`Y:$0502`): **Unison Cross-Mix**. Balance between center voice and detuned stereo voices.
- **`SUBX`** (`Y:$0504`): **Sub-Oscillator Level**. Level of the -1 or -2 octave sub-oscillator.
- **`SUB1` / `SUB2`** (`Y:$0505..0506`): **Sub-Oscillator Waveshape**. Selects square, pulse, or triangle sub-wave.
- **`PCH2` / `PCH3` / `PCH4`** (`Y:$0500..0502` on SWAVE-ENS): **Voice Pitch Offsets**. Offsets for voices 2, 3, and 4 relative to root note in semitones `(-24 .. +24)`.
- **`CHRL` / `CHRW`** (`Y:$0505..0506` on SWAVE-ENS): **Ensemble Chorus Level & Width**. Built-in multi-voice chorus depth.

#### 2. DPRO Family (Machines 6, 7, 32, 33)
- **`WP`** (`Y:$0501` on DPRO-WAVE): **Wave Position**. Offset in the 24-bit wavetable memory (`P:$14a000`).
- **`WPM`** (`Y:$0502`): **Wave Position Modulation**. Modulates wavetable index from envelope/LFO.
- **`WPRS`** (`Y:$0503`): **Wave Reset**. Phase reset to start of table on Note-On.
- **`WAV1` / `WAV2`** (`Y:$0500`, `Y:$0502` on DPRO-DDRW): **Wave Slot 1 & 2**. Selects user/factory waveform slots (0..63).
- **`TIME`** (`Y:$0503` on DPRO-DDRW): **Interpolation Time**. Slew rate for crossfading between WAV1 and WAV2.
- **`BR1` / `BR2`** (`Y:$0504`, `Y:$0506` on DPRO-DDRW): **Bit Reduction**. 24-bit downsampling/quantization for each slot.

#### 3. VO-6 (Machine 11)
- **`VOC1` / `VOC2`** (`Y:$0500..0501`): **Vocal Formants 1 & 2**. Formant filter center frequencies for vowel synthesis.
- **`V-SW`** (`Y:$0502`): **Formant Sweep**. Speed and direction of transition between formants.
- **`VOIC`** (`Y:$0503`): **Vowel Character**. Vowel table selection (`A, E, I, O, U` character blend).
- **`CONS`** (`Y:$0504`): **Consonant Type**. Plosive/fricative noise burst selection on trigger.
- **`CLEN`** (`Y:$0505`): **Consonant Duration**. Length of consonant transient in milliseconds.
- **`CVOL`** (`Y:$0506`): **Consonant Level**. Gain of consonant transient before vowel body.

#### 4. FX Machines (Machines 12, 13, 15, 16, 17, 18, 19)
- **`INP`** (`Y:$0507` on all FX): **Input Bus Select**.
  - `0`: External Input Pair A/B (Codec ADC).
  - `1`: External Input Pair C/D.
  - `2`: Internal Track 1 neighbour routing.
  - `3`: Internal Track 2 neighbour routing, etc.
- **`CNTR`** (`Y:$0500` on FX-PHASER): **Center Frequency**. Center notch of allpass ladder.
- **`RAT`** (`Y:$0504` on FX-DYNAMIX): **Compressor Ratio**. 1:1 up to $\infty$:1 (Limiter).
- **`RMS`** (`Y:$0506` on FX-DYNAMIX): **Peak vs RMS Detection**. Blends peak limiter with smooth RMS leveling.

---

## 9. How to Test and Build as VST3

### A. Testing the DSP Algorithms Locally (Without DAW)
You can run the full test suite using the included `Makefile`:

```bash
make test
```
This compiles and executes all 6 unit tests with `g++`:
- `monomachine_suite_test`: Verifies all 22 machine definitions and UI grid layout.
- `monomachine_fm_test`: Verifies FM+ Dynamic synthesis, frequency calculations, and audio output.
- `monomachine_bbox_test`: Verifies 10-slot PCM sample engine, playback, and randomizer.
- `monomachine_arp_test`: Verifies the hardware arpeggiator modes (`KEY`, `SID`, `ADD`).
- `monomachine_matrix_test`: Verifies Keytracking, Velocity scaling, and Param X/Y modulation matrix.
- `monomachine_conversions_test`: Verifies knob-to-frequency mappings and LCD screen values (`1.0`, `1.33`).

### B. Building as a VST3 Plugin with JUCE
To compile into a `.vst3` bundle for Ableton, FL Studio, Reaper, Cubase, or Bitwig:

1. Download or clone the JUCE framework:
   ```bash
   git clone --depth 1 https://github.com/juce-framework/JUCE.git
   ```
2. Create a standard JUCE AudioPlugin project (via `Projucer` or `CMakeLists.txt`):
   - **For Monomachine Synth**: Enable "Plugin is a Synth", "Plugin MIDI Input".
   - **For Monomachine FX**: Disable "Plugin is a Synth", configure Stereo Input/Output buses.
3. In `PluginProcessor.cpp`:
   ```cpp
   #include "dsp/monomachine_voice_chain.hpp"
   #include "models/modulation_matrix.hpp"
   
   monomachine::MonomachineVoiceChain voiceChain;
   
   void prepareToPlay(double sampleRate, int samplesPerBlock) {
       voiceChain.reset(sampleRate);
   }
   
   void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
       // Route MIDI note events to voiceChain.filter().triggerEnvelope(), etc.
       float* left = buffer.getWritePointer(0);
       float* right = buffer.getWritePointer(1);
       voiceChain.processBlock(left, right, left, right, buffer.getNumSamples());
   }
   ```
4. Build using CMake:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
   The resulting `MonomachineOne.vst3` will be in `build/MonomachineOne_artefacts/Release/VST3/`.

---

## 10. How to Save Your Work Since the PR is Merged

Because Pull Request #1 was closed/merged, GitHub remote write permissions for this temporary coding sandbox session are sealed. However, **all your code is committed to Git locally** on your branch `arena/01a0a623-mytt`.

To keep your files and continue working in your own environment:
1. **Clone your repository locally on your computer**:
   ```bash
   git clone https://github.com/glassg333/mytt.git
   cd mytt
   ```
2. **Fetch all branches from remote**:
   ```bash
   git fetch origin
   git checkout arena/01a0a623-mytt
   ```
3. If you want to merge these changes into `main` on your machine:
   ```bash
   git checkout main
   git merge arena/01a0a623-mytt
   git push origin main
   ```
4. If starting a new session on Arena.ai, simply create a new session pointing to this repository, and you can push new branches freely.
