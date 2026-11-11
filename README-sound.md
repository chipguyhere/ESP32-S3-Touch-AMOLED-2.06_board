# ESP32-S3-Touch-AMOLED-2.06 — sound

Technical notes on the audio path, for a developer who wants a beep, a sample,
speech, or voice in, without breaking anything else. The board has a proper codec
for the speaker, a separate multi-channel microphone ADC with an echo-cancellation
reference, two analog MEMS microphones and a class-D amplifier. The board libraries
**don't ship an audio driver**; this file gives the chain, the numbers and the traps.
It was read from the schematic and the manufacturer's `08_ES8311` example. **Nothing
has been played yet** (both chips were seen answering on I²C on 2026-10-05), so it is
heavy on `TODO (bench)`. The
reasoning is in [README-engineering.md](README-engineering.md#sound).

## Sound characteristics at a glance

### Signal chain

**What is the output device?** A speaker on the 2-pad connection **P5/P6**, driven
by the amplifier below. The manufacturer's `08_ES8311` example plays a melody at 85 %
volume, so a speaker is evidently fitted on shipped units (inferred). Impedance and
size unknown. **TODO (bench):** confirm it's there and measure it.

**What generates the analog signal?** An **ES8311** mono codec on the shared I²C bus
at **0x18** (CE pulled low), fed by I²S:

| I²S signal | GPIO | ES8311 pin | ES7210 pin |
|---|---|---|---|
| MCLK | 16 | MCLK | MCLK |
| BCLK | 41 | SCLK | SCLK |
| WS / LRCK | 45 | LRCK | LRCK |
| DOUT (ESP32 → codec) | 40 | DSDIN | — |
| DIN (mic ADC → ESP32) | 42 | — (its ASDOUT is unconnected) | SDOUT1, via 51 Ω (R47) |

Both chips share MCLK, BCLK and WS: one I²S port, playback out on GPIO40 and capture
in on GPIO42. Each clock line has a 22 pF capacitor to ground at each chip.

**Is there an amplifier?** Yes, an **NS4150B** mono class-D, powered from the main
**3.3 V** rail (VCC3V3). Its **enable (`CTRL`) is GPIO46**, through a 0 Ω link with a
**10 kΩ pull-down**, so the amplifier is **off unless a sketch turns it on**:
`board.ampEnable(true)`. Its input is differential from the codec's `OUTP/OUTN`,
through **0.1 µF + 150 kΩ** per side (C40/R52, C45/R53). With an internal feedback
resistor of the same order, the gain is about **2 V/V (6 dB)** — inferred. **TODO
(datasheet):** confirm the NS4150B's internal resistance and gain equation.

**What sits between source and amplifier?** The codec's own output coupling
(1 µF, C44/C46) with 22 pF filtering to ground, then the 0.1 µF + 150 kΩ input
network. The amplifier's outputs pass 0 Ω links (L4/L5, footprints for ferrites) to
the speaker, with unfitted filter-capacitor footprints there.

**Mono or stereo?** **Mono**: one codec channel, one amplifier, one speaker.

**Is there an input path?** Yes, see below.

### Microphones

- **ES7210** 4-channel ADC at **0x40** (AD0/AD1 strapped low). Its **whole supply,
  digital included, comes from ALDO1** (`A3V3`). Measured consequence: with ALDO1
  off, the unpowered ES7210 **clamps the shared I²C bus** and nothing on it answers
  — see [Power interaction](#power-interaction). ALDO1 is on from power-up and the
  board library keeps it on.
- **MIC1, MIC2:** two **analog MEMS microphones** powered from the ES7210's
  `MICBIAS12` output. Each output is AC-coupled (1 µF) into the ES7210's MIC1P / MIC2P,
  with the N input AC-coupled to analog ground. Effectively single-ended.
- **MIC3: echo reference.** The codec's `OUTP/OUTN` speaker drive is fed back through
  a divider and filter network into MIC3P/N. That gives echo cancellation a copy of
  what the speaker is playing.
- **MIC4:** both inputs are AC-coupled to analog ground. Unused.
- The ES8311's own microphone input is **not connected**, and neither is its ADC
  output. **All capture goes through the ES7210.**

### Getting samples out (and in)

**Which driver?** The core has no built-in driver for either codec. The practical
routes are:

- **ESP-IDF's `esp_codec_dev`**, which supports both the ES8311 and ES7210.
- **The manufacturer's `es8311.c` / `es8311.h`** (from Espressif's component), as in
  `08_ES8311`, plus the core's `ESP_I2S` for the stream. It needs a `TwoWire`-style
  I²C port, so do all of its calls from one task, or under a `ThreadsafeWire::Guard`.
- There is **no beep shortcut**. The speaker is behind the codec, not a GPIO.

A starting configuration, following the manufacturer's example (not a supported API):

```cpp
#include <ESP_I2S.h>
I2SClass i2s;

board.begin();
board.audioPower(true);                         // ALDO1: codec analog + mic ADC
i2s.setPins(/*bclk*/ 41, /*ws*/ 45, /*dout*/ 40, /*din*/ 42, /*mclk*/ 16);
i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
// ...configure the ES8311 over I2C at 0x18 (MCLK = 256 x fs from the MCLK pin)...
board.ampEnable(true);                          // only once the codec is streaming
```

The manufacturer's example runs 16 kHz, 16-bit, MCLK = 256 × fs. Its "echo" loop
configures only the ES8311's microphone, which is not wired. So the samples it
reads back on GPIO42 come from an **unconfigured ES7210**, and it most likely echoes
silence. **TODO (bench).** To record, configure the ES7210.

**Sample rates / bit depth:** both chips handle 8–96 kHz, 16–32-bit. The dividers are
relative to MCLK, so keep MCLK at 256 × fs. Format is 16-bit signed PCM. The ES8311 is
mono and takes the left slot; send the same sample in both slots.

**Conflicts:** GPIO45 (WS) and GPIO46 (amplifier enable) are strapping pins. Both are
fine after boot. Never add a pull-up to GPIO45: it must be low at reset (see
[README-board.md](README-board.md#strapping-pins)). Nothing else shares these pins.

**Volume:** in the codec. The ES8311's DAC volume register gives 0.5 dB steps, and the
ES7210 has per-channel PGA gain. The amplifier's gain is fixed.

### Limits that keep it clean

**Amplitude:** with ~6 dB of gain on a 3.3 V supply, a full-scale codec output will
clip the amplifier. **TODO (bench):** raise a sine's level until the output distorts,
and note the codec volume; that is the ceiling.

**Pops:** enable in this order: codec configured and streaming zeros, *then*
`ampEnable(true)`. Disable in reverse: `ampEnable(false)` *before* stopping I²S.
**TODO (bench):** listen for pops either way.

**Idle:** turn the amplifier off when not playing. That is cheap, and it is the only
reliable cure for hiss and WiFi ticking. **TODO (bench):** idle noise with the amp on.

**Bandwidth:** the 0.1 µF / 150 kΩ input high-pass is ~10 Hz, so it is not a limit.
The small speaker is.

### Power interaction

- **The audio rail must never be switched off.** ALDO1 feeds the ES8311's analog side
  and the *entire* ES7210. Measured (2026-10-05): with ALDO1 off, the unpowered ES7210
  clamps SDA/SCL and every chip on the I²C bus stops answering — including the PMIC,
  so ALDO1 can't be turned back on, and resetting the ESP32 doesn't help. Only
  removing all power recovers. So `board.audioPower(false)` is a no-op returning
  `false`, and `power.setRail(ALDO1, false)` refuses. **To save power, put the codecs
  to sleep with their own power-down registers** (ES8311 and ES7210 both have them)
  and keep the amplifier disabled.
- With the amplifier disabled (the default), it draws only its shutdown current.
- Nothing about audio is a wake source. Current figures belong in the power library's
  consumption table: **TODO (bench)**.

### Recommendations

- **`setup()` order:** `board.begin()` → `board.audioPower(true)` → I²S begin → codec
  registers → `board.ampEnable(true)` when you actually play.
- **Minimal beep:** the above, then stream a 16-bit sine at 16 kHz into both slots.
  Finish by writing zeros briefly, then `board.ampEnable(false)`.
- **Voice in:** bring up the ES7210 for MIC1+MIC2 (plus MIC3 if you want the echo
  reference), same clocks, read from GPIO42.
- **The two family calls:** `bool board.audioPower(bool on)` — `on=true` sets ALDO1
  to 3.3 V and enables it, returning `true` (a switch exists). `on=false` is
  deliberately a **no-op returning `false`**: the switch exists but turning it off
  jams the I²C bus (above). `void board.ampEnable(bool on)` drives GPIO46 and is a real
  switch.
- **TODO for the library family:** a sound helper that hides the ES8311/ES7210
  bring-up and offers `beep()` / `play16()` / `mute()`.

### Playing MP3s with ESP8266Audio

The library's stock `AudioOutputI2S` needs four fixes before it behaves on a codec
board. Subclass it and:

1. **Keep `ConsumeSample()` non-blocking.** If it waits for DMA space, the decoder's
   `loop()` never returns and the rest of the task stops. Pace the decoder task
   instead: `vTaskDelay(1)` when nothing was consumed.
2. **Keep the I²S channel alive across tracks.** Make `begin()` idempotent and `stop()`
   a no-op, so MCLK never stops and the codec doesn't pop or relock between songs.
3. **Send zeros on underrun.** Set `auto_clear = true` in the channel config, otherwise
   the DMA repeats its last buffer as a buzz.
4. **Mono codec:** mix (L+R)/2 into both slots, and do volume in the ES8311's DAC
   register rather than with `SetGain()`.

Use the pins above, keep MCLK at 256 × fs, and follow the amplifier enable order.
