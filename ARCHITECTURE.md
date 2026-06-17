# Wing Drum — Firmware Architecture & Functionality

Wing Drum ([phonicbloom.com/drum](https://phonicbloom.com/drum/)) is a polyphonic, sample-based electronic percussion instrument built around an ESP32 microcontroller. It has nine capacitive touch pads arranged in a centre-plus-eight-wing layout; touching a pad plays an audio sample pitched to a configurable musical note, with up to nine simultaneous voices.

---

## 1. Hardware Overview

| Component | Part / Detail |
|-----------|--------------|
| MCU | ESP32 (Xtensa LX6 dual-core, 240 MHz) |
| Audio codec | Texas Instruments TLV320AIC3104 |
| Codec control | I²C at 400 kHz (GPIO 21 SDA, 22 SCL) |
| Codec audio | I²S (I2S_NUM_0), 16-bit stereo, 48 kHz via APLL |
| Accelerometer | WMA6981 3-axis (I²C address 0x12) |
| Touch pads | 9 × ESP32 capacitive touch sensor inputs |
| Physical buttons | 5 buttons: Power/Scale, Metal, Wood, Minus, Plus |
| LEDs | 8 LEDs via PCA9552 I²C LED expander (2 × 4-LED quadrants) |
| MIDI out | UART2, GPIO 19, standard 31 250 baud |
| Flash | 16 MB SPI flash (custom partition table) |

The codec reset line is GPIO 23. Power is latched on in software via a hold signal on GPIO 16, allowing the firmware to control its own shutdown.

---

## 2. Flash Partition Layout

```
Offset      Name        Size    Purpose
──────────────────────────────────────────────────
0x9000      nvs         80 KB   NVS — user/runtime settings
0x1d000     otadata      8 KB   OTA state
0x1f000     phy_init     4 KB   Wi-Fi PHY calibration data
0x20000     config      64 KB   config_drum.txt (read-only instrument config)
0x30000     factory      1 MB   Application binary
0x130000    samples     ~15 MB  Raw 16-bit mono PCM audio samples
0xfec000    nvs_bak     80 KB   NVS backup copy
```

The `samples` partition at 0x130000 holds up to 16 raw audio samples (16 individual recordings totalling ~15 MB, mapped directly as a flash memory region and read during playback).

---

## 3. Firmware Architecture

### 3.1 Module Map

```
main/
├── main.cpp              Entry point, boot sequence, patch select loop
├── main.h                Top-level includes
├── board.h               Board variant selector (BOARD_WINGDRUM_V1)
├── glo_config.{h,c}      Config file parser, NVS settings, patch/scale loader
├── ChannelsDef.{h,cpp}   Audio modes: sample drum, reverb, white noise, pass-through
├── InitChannels.{h,cpp}  Per-patch audio engine init/deinit
├── Interface.{h,cpp}     Program state reset helpers
│
├── dsp/
│   ├── SampleDrum.{h,cpp}  Polyphonic sample playback DSP loop
│   └── Reverb.{h,cpp}      Polyphonic reverb DSP loop
│
└── hw/
    ├── init.{h}            FreeRTOS/ESP-IDF includes, I²C helpers, hardware check
    ├── codec.{h,c}         TLV320AIC3104 driver (init, volume, EQ, AGC, mute)
    ├── signals.{h,c}       Signal globals (sample buffers, echo, reverb state)
    ├── gpio.{h,c}          GPIO pin definitions, buttons, LED macros, power control
    ├── ui.{h,c}            Button scanning, UI event processing, scale/patch navigation
    ├── midi.{h,c}          MIDI note/PB/CC output, note-to-frequency conversion
    └── Accelerometer.{h,cpp} WMA6981 driver, axis reading, calibration
```

### 3.2 Boot Sequence (`main.cpp`)

1. Power latch — hold the power-on signal so the device can stay alive after the user releases the power button.
2. Codec hardware reset.
3. I²C bus init + hardware presence check (detects missing peripherals and blinks an error pattern if found).
4. Echo buffer allocation (static, in IRAM).
5. Accelerometer init (continuous background read mode).
6. FreeRTOS task creation: `process_buttons_controls_drum`, `process_ui_events`, `touch_pad_process` — all pinned to Core 1.
7. MIDI UART init.
8. Settings load: global settings from config partition, persistent user settings from NVS.
9. Tuning coefficients and patch lists parsed from config.
10. Scale memory allocated for all patches.
11. Power-on LED animation.
12. Accelerometer calibration (if enabled).
13. I²S + MCLK start, codec init, AGC configured.
14. Wait for touch pad calibration to complete.
15. Enter **patch select loop**.

### 3.3 Patch Select Loop

The main loop cycles through instrument patches in response to button events:

```
event_next_channel
  ├── EVENT_NEXT_CHANNEL_METAL (101) → advance metal patch pointer → sample_drum()
  ├── EVENT_NEXT_CHANNEL_WOOD  (102) → advance wood patch pointer  → sample_drum()
  ├── EVENT_NEXT_CHANNEL_BOTH  (103) → reverb mode → channel_reverb()
  └── EVENT_NEXT_CHANNEL_PWR_OFF (109) → power-off animation → drum_shutdown()
```

After each engine returns (triggered by the next button event), `channel_deinit()` releases the patch's allocated resources.

---

## 4. FreeRTOS Task Structure

All tasks run on **Core 1** at equal priority (10). The audio DSP also runs on Core 1 inside `app_main`, meaning audio computation and UI tasks share the core and are interleaved via FreeRTOS scheduling. Core 0 runs the idle task and FreeRTOS internals.

| Task | Stack | Role |
|------|-------|------|
| `process_buttons_controls_drum` | 4 KB | Button scan & debounce at 10 ms intervals, generates `event_next_channel` |
| `process_ui_events` | 4 KB | Higher-level interpretation: volume, EQ, scale selection, micro-tuning |
| `touch_pad_process` | 4 KB | Capacitive touch read, threshold calibration, `new_note()` calls |
| `app_main` (audio loop) | (main stack) | Per-sample DSP: sample mixing, echo, I²S write |
| `process_accelerometer` | (inside driver) | Continuous 3-axis read at ~20 ms, updates `acc_res[]` |

---

## 5. Touch Pad and Note Generation

Nine ESP32 capacitive touch channels map to the nine drum pads. At startup the firmware self-calibrates baseline capacitance values. When a pad is touched:

- The raw touch value is compared against a per-pad threshold (configurable via `TOUCHPAD_LED_THRESHOLD_A_MUL/DIV`).
- The magnitude of the touch (above-threshold delta) becomes the note **velocity**.
- Three velocity tiers (`SPLIT_TIMBRE_NOTE_THRESHOLD1/2`) select which **timbre segment** of a multi-segment sample to play.
- `new_note()` posts the event (pad index, MIDI note number, velocity) into shared variables read by the DSP loop.

---

## 6. Audio DSP — Sample Drum Engine (`SampleDrum.cpp`)

The engine is a nine-voice polyphonic sample player running in a tight per-sample loop.

### 6.1 Voice Model

Each of the nine voices carries:

| Variable | Meaning |
|----------|---------|
| `s_ptr[v]` | Current position in the sample (integer, in samples) |
| `s_step[v]` | Playback rate coefficient (determines pitch) |
| `s_note[v]` | MIDI note number currently playing (0 = inactive) |
| `s_end[v]` | End position (sample length for this voice's timbre segment) |
| `mixing_volumes[v]` | Amplitude (derived from touch velocity) |

A voice is inactive when `s_ptr[v] < 0`.

### 6.2 Pitch Shifting

The sample is stored at a known base pitch (e.g., A3 = 48 000 Hz at 48 kHz → `S_STEP_DEFAULT_A3 = 48000/48000 = 1.0`). Each pad's assigned MIDI note determines the playback rate:

```cpp
s_step[v] = MIDI_note_to_coeff(midi_note) * tuning_coeff * micro_tuning[note];
```

`MIDI_note_to_coeff()` computes `freq(note) / freq(A3)` using the equal-temperament semitone ratio (2^(1/12)). The result is a fractional step: notes above A3 advance faster through the sample (higher pitch), notes below advance slower.

The sample pointer is floating-point; the integer and fractional parts are split each cycle for **linear interpolation** between adjacent samples, eliminating staircase aliasing:

```cpp
f_ptr = (float)s_ptr[v] * s_step[v];
i_ptr = (int)f_ptr;
frac  = f_ptr - i_ptr;
sample_mix += ((1 - frac) * sample[i_ptr] + frac * sample[i_ptr + 1]) * mixing_volumes[v];
```

### 6.3 Voice Slot Assignment (`find_next_slot`)

When a new note arrives, the engine selects a slot using a priority cascade:

1. If ≥ 5 slots already play the same note → steal the oldest of those.
2. Otherwise look for an empty slot (s_note == 0).
3. Otherwise find any slot already playing the same note and steal it (retrigger).
4. Otherwise find two slots both playing the same note and steal the one that has progressed further.
5. Otherwise steal the oldest slot playing a note ≥ the new note.

When a slot is stolen mid-playback, the instantaneous sample amplitude is added to a `voice_override_ramp` that decays exponentially (coefficient 0.99 per sample), preventing an audible click.

### 6.4 Multi-Timbral Samples

Several samples are stored as **concatenated segments** representing different recording takes or dynamics. The `[timbre_segments]` block in `config_drum.txt` defines for each sample:

- Byte offset and length of each segment.
- The base note for that segment (used to compute a tuning correction so all segments pitch-shift correctly).
- A velocity mapping table: which segment to use at soft / medium / hard touch (up to 3 velocity tiers, up to 4 segments).

The fine-tuning adjustment (`timbre_parts_tuning[]`) corrects for any recording pitch difference between segments, computed as `freq(base_note) / freq(reference)`.

### 6.5 Echo / Delay

An echo buffer of fixed maximum length is allocated at boot (`init_echo_buffer()`). The buffer length and feedback level are runtime-configurable:

- `echo_dynamic_loop_length` — delay time in samples; adjustable via buttons or (optionally) accelerometer tilt.
- `ECHO_MIXING_GAIN_MUL / DIV` — feedback fraction, smoothly ramped to a target value at 1 kHz.

The echo is applied in stereo — both L and R channels are processed identically through `add_echo()`.

### 6.6 Reverb Mode

When the user presses both Wood+Metal buttons together, `channel_reverb()` replaces sample playback with `decaying_reverb()` from `Reverb.cpp`. This is a polyphonic comb-filter / feedback-delay-network style reverb. The reverb buffer length (`BIT_CRUSHER_REVERB_DEFAULT = REVERB_BUFFER_LENGTH/3` ≈ 1600 samples at 48 kHz ≈ 33 ms) is adjustable. Up to 9 extra delay buffers (`reverb_buffer_ext[]`) extend the reverb at octave intervals above and below.

---

## 7. Patch and Scale System

### 7.1 Patches

Patches are loaded from the `[default_order]` block in `config_drum.txt`. Each entry specifies:

```
type:sample_number, scale1, scale2, ...
```

- **Wood patches** (5): samples 7, 8, 1, 2, 9 — lower-pitched wooden drums and djembes.
- **Metal patches** (11): samples 13–16, 3–6, 10–12 — steel tongue drums, hang drums, glass instruments, glockenspiel, natraja.

Each patch can list up to 8 scale names; the user cycles through them at runtime.

### 7.2 Scales

A scale assigns a MIDI note to each of the nine pads. The format is:

```
scale-name: center, pad1, pad2, pad3, pad4, pad5, pad6, pad7, pad8
```

(Centre pad followed by eight wing pads clockwise from top-right.)

Notes are written in scientific pitch notation (`a3`, `c#4`, `a#2`, etc.); the firmware parses them to integer MIDI note numbers. Over 50 named scales are defined, spanning modes and world music tunings:

- **Diatonic modes**: major, minor, harmonic minor, Phrygian, Lydian, Mixolydian
- **World scales**: Kurd, Celtic minor, Akebono, Annaziska, Pygmy, Ionian, Greek Mixolydian
- **Pentatonics and custom voicings**

### 7.3 Micro-Tuning

Each note within a scale can be fine-tuned independently in steps of 10 cents (120th root of 2 ≈ 1.005793). Up to 8 × 9 = 72 micro-tuning values are stored per patch, persisted to NVS. This allows precise intonation matching to acoustic instruments.

### 7.4 Sample Tuning Coefficients

Each of the 16 samples is recorded at a specific pitch. The `[samples_tuning]` block provides a coefficient to normalise each to A4/A5:

```
13: 1.33484   # steel_01.wav, E4 → shifted 5 semitones up to A5
16: 0.74915   # thaigong.ff.wav, C#2 → shifted 5 semitones down
```

This coefficient is multiplied into `s_step[]` so the scale note assignments produce correct absolute pitches regardless of which sample is loaded.

---

## 8. Configuration and Persistent Storage

### 8.1 Config File (`config_drum.txt`)

Stored in the `config` flash partition (64 KB, at 0x20000). Parsed at boot by `glo_config.c`. Sections:

| Section | Contents |
|---------|----------|
| `[global_settings]` | Tuning range, auto-power-off timeouts, AGC params, codec volume, micro-tuning step, touch threshold coefficients |
| `[default_order]` | Ordered list of wood and metal patches with their scale assignments |
| `[scales]` | ~50+ named musical scales |
| `[samples_map]` | Sample index → byte length (positions computed cumulatively) |
| `[timbre_segments]` | Per-sample multi-segment definitions and velocity maps |
| `[samples_tuning]` | Per-sample pitch normalisation coefficients |

### 8.2 NVS (Non-Volatile Storage)

Runtime user adjustments persisted to the `nvs` flash partition via `nvs_flash`. Includes:

- Analog and digital volume levels
- EQ bass and treble settings
- AGC enable/disable and max gain
- Mic bias setting
- Accelerometer X/Y calibration offsets
- Echo loop length step
- Per-patch custom scales (override of config defaults)
- Per-patch micro-tuning tables
- Self-test pass flag

Settings are written back to NVS after a debounce timer (2–5 seconds) to limit flash wear.

---

## 9. User Interface

### 9.1 Physical Buttons

| Button | GPIO | Active | Function |
|--------|------|--------|----------|
| BT1 — Power/Scale | 35 | High | Power on/off; scale cycling |
| BT5 — Metal | 36 | Low | Next metal patch |
| BT4 — Wood | 39 | High | Next wood patch |
| BT2 — Minus | 25 | Low | Volume down / settings decrease |
| BT3 — Plus | 26 | Low | Volume up / settings increase |

### 9.2 Button Combinations and Long Presses

The UI engine (`ui.c`) interprets sequences of short and long presses to access secondary functions without a display:

| Combination | Action |
|-------------|--------|
| Metal + Wood (both, short) | Enter reverb mode |
| Power + Metal long | Enter scale settings mode |
| Minus + Plus (both, long) | EQ context menu (bass/treble) |
| Power + Minus long | Adjust delay length |
| Power + Plus long | Fine-adjust delay length |
| Long Metal or Wood | Copy/paste scale between patches |
| Scale mode + Minus/Plus | Navigate notes within scale |
| Scale mode + Metal/Wood | Adjust note pitch (micro-tuning) |

Power-off requires holding BT1 until the LED animation completes; releasing early cancels shutdown.

### 9.3 LEDs

Eight LEDs are driven through a PCA9552 I²C LED driver (two four-LED quadrants). Each LED has three states: ON, OFF, BLINK-SLOW (B1), BLINK-FAST (B2), mapped to visual feedback for:

- Active pad (lit while sustaining)
- Current scale position
- Settings navigation
- Power-on / power-off animation (sequential sweep)
- Error states (alternating blink patterns)

### 9.4 Auto-Power-Off

Inactivity timers auto-shutdown the device:
- **Sample modes**: 5 minutes (`AUTO_POWER_OFF_TIMEOUT_ALL = 600` × 500 ms intervals)
- **Reverb mode**: 120 minutes (`AUTO_POWER_OFF_TIMEOUT_REVERB = 14400`)

Any touch event or button press resets the timer. The timer drives a volume ramp-down before cut-off (`AUTO_POWER_OFF_VOLUME_RAMP = 60` steps at 500 ms each = 30 seconds fade).

---

## 10. MIDI Output

MIDI is sent via UART2 (GPIO 19) at 31 250 baud, single-direction (output only).

| Message | Trigger | Rate |
|---------|---------|------|
| Note-on (0x90) | Pad touch | On event |
| Note-off (0x80) | Pad release | On event |
| Pitch Bend (0xE0) | Accelerometer X-axis | 10 Hz |
| CC #1 — Modulation (0xB0) | Accelerometer Y-axis | 4 Hz |

MIDI pitch bend reflects the physical tilt of the instrument along its long axis, calibrated to the resting position. CC modulation reflects tilt along the cross axis. A deadband of ±10 raw units around the calibrated centre suppresses jitter when the instrument is stationary.

Up to three simultaneous MIDI notes are sent per touch event (chord output, `MIDI_OUT_POLYPHONY = 3`).

---

## 11. Accelerometer

The WMA6981 is read continuously by a background task at ~20 ms intervals (50 Hz). The three raw axes are scaled to float values and stored in `acc_res[3]`.

- **Calibration**: On power-up (if enabled), 25 consecutive readings are averaged to establish a neutral position. The calibrated offsets are saved to NVS.
- **MIDI mapping**: X-axis → pitch bend (centre = 64, range 0–127), Y-axis → CC modulation.
- **Octave switching**: Code exists (guarded by `OCTAVE_SWITCH_X/Y` defines, currently disabled) to change the active scale up or down an octave when the drum is tilted past thresholds (±25 units on X, ±20 units on Y).

---

## 12. Data Flow Summary

```
Touch pads ──→ touch_pad_process task ──→ new_note() ──→ note_updated flags
                                                              │
Accelerometer ──→ acc_res[] ─────────────────────────→ MIDI PB/CC output
                                                              │
                                                    DSP loop (app_main)
                                                              │
                                          ┌───────────────────┼────────────────────┐
                                    Read note event     Sample flash         Echo buffer
                                          │               (SPI mapped)            │
                                    find_next_slot()          │                    │
                                          │            Linear interpolation        │
                                    s_step = MIDI_note_to_coeff × tuning          │
                                          │                   │                    │
                                    Mix 9 voices ────────────→ sample_mix          │
                                                              │                    │
                                                          add_echo() ←────────────┘
                                                              │
                                                     i2s_write() → TLV320AIC3104 → audio out
```
