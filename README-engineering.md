# ESP32-S3-Touch-AMOLED-2.06 — board engineering notes

The complete reading of the sheet (every part, every net, the whole GPIO table) is in
this library's [`schematic-description.md`](schematic-description.md). This file keeps
only the reasoning behind [`README-board.md`](README-board.md),
[`README-sdcard.md`](README-sdcard.md) and [`README-sound.md`](README-sound.md).

**Source:** the manufacturer's Altium schematic `ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0.pdf`
(one sheet). Its connectivity was rebuilt by machine from the PDF's vector geometry
and hidden pin markers, then checked block by block against the drawing; values were
read by eye. Also used: the manufacturer's Arduino examples and `pin_config.h`.
Everything is **inferred** unless marked measured, and nothing has been measured. The
display's own derivations are in the
[display library's engineering file](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README-engineering.md),
and the battery/charger/power-key reasoning is in the
[power library's](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README-engineering.md).

## Board

### Module, flash and PSRAM

U2 is an **ESP32-S3R8**: 8 MB of octal PSRAM in the package. The PSRAM takes GPIO26
(`SPICS1`) and GPIO33–37; U2 pins 28 and 38–42 are unconnected on the sheet, which is
right for an R8. GPIO47/48 (U2 pins 36/37, `SPICLK_N/P`) are also unconnected. They
would be usable GPIOs on an R8, but nothing reaches them.

The flash, U3, is printed as **GD25Q256EYIGR**: 32 MB, quad, on the usual
`SPICS0/SPICLK/SPID/SPIQ/SPIWP/SPIHD` pins. It is powered from the S3's **`VDD_SPI`**
output (U3.8 ← U2.29), with SPIHD decoupled by an unfitted C5. Every `sdkconfig` in the
manufacturer's ESP-IDF examples sets `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`, but the
part fitted is the 32 MB one the sheet names. Measured on one board (2026-10-05):
`esp_flash_read_id()` returns **0xC84019** (GigaDevice, GD25Q256 family) and
`esp_flash_get_physical_size()` **32 MB**. The manufacturer's firmware simply doesn't
use the top half. The READMEs recommend the 32 MB settings; 16 MB still works.

### Reset, boot and the PMIC's power-good

`CHIP_PU` (EN) is driven by the AXP2101's **PWROK** (UP1.29), with R9 10 kΩ to VCC3V3
and C15 100 nF to ground. The PMIC therefore holds the S3 in reset until its rails are
good. There is no RC-only reset and no reset button: a hung board is recovered by the
PMIC's PWR long-press power-off, not by EN.

BOOT (Key1) pulls GPIO0 to ground, with R8 10 kΩ pull-up and C14 100 nF.

### The PWR button path

```
VCC3V3 ─ R11 10k ─┬─ SYS_OUT ─ GPIO10
                  │
              T1 drain (BSS138)
              T1 gate ─ R16 1k ─┐
              T1 source ─ GND   │
                                ├─ node ── Key3 (PWR) ── GND
AXP2101 PWRON ─┬─ RP7 510R ─────┘
               └─ CP10 1 nF ─ GND
```

With PWR up, the node sits at whatever the AXP2101's internal PWRON pull-up sets it
to. That keeps T1's gate high, T1 on, and GPIO10 **low**. With PWR down, the node and
the gate go to ground, T1 turns off, and R11 pulls GPIO10 **high**. Hence
"HIGH while held" — **measured** (2026-10-05): GPIO10 reads 0 idle and 1 while held,
cleanly, with edges matching the PMIC's own press/release events. Notes:

- The PWRON idle level is set inside the AXP2101. A pull-up to a low internal rail
  would leave the BSS138 (V<sub>GS(th)</sub> 0.8–1.5 V) only marginally on; the digital
  reads show it is on enough. An analog scope trace is still worth having
  (**TODO (bench)**), but the logic levels are settled.
- RP7 (510 Ω) and CP10 form the PMIC's own debounce/ESD network. The ESP32's sense is
  taken on the button side of RP7, so it sees the raw contact.

GPIO10 is an RTC GPIO, so the press can wake deep sleep (ext1).

### The AXP2101 as the schematic configures it

What the sheet actually wires, as opposed to what its "Parameter Set" table says:

| Output | Wired to | Table says |
|---|---|---|
| DCDC1 (LX1 via LP1 1 µH, FB1) | **VCC3V3**: everything | VCC3V3 ✓ |
| DCDC2/3/4 (VIN/LX/FB pins) | **nothing**: all pins floating | 0.9 / 1.2 / 1.8 V |
| DCDC5 (SW via LP2 1 µH) | `Vsys/DCDC5` node, FB via unfitted RP4 | NC |
| ALDO1 | **A3V3** (audio analog) | VL1 3.3 V |
| ALDO2 | 10 kΩ pull-up on the panel's `DSI_PWR_EN` | VL2 3.3 V |
| ALDO3 | vibration motor via R7 0 Ω | "VCC3V" |
| ALDO4 | **nothing** | VL3 1.8 V |
| BLDO1/2, CPUSLDO, DLDO1/2 | **nothing** | 1.2 / 2.8 / 1.2 V |
| RTCLDO | **VCC-RTC**: PCF85063, AXP IRQ pull-up | — |

The table is evidently carried over from a reference design that used more rails.
The rail voltages that matter here come from the AXP2101's factory OTP. **Measured
after removing all power** (2026-10-05): DCDC_EN 0x0F (DCDC1–4 on), LDO_EN0 0xFF and
LDO_EN1 0x01 — **every LDO on**, the unconnected ones included — with ALDO1 3.3 V,
ALDO2 3.3 V, ALDO3 3.0 V, ALDO4 1.8 V, BLDO1 1.2 V, BLDO2 2.8 V, DCDC1 3.3 V. So the
OTP voltages match the parameter table where it overlaps. The board library still
sets ALDO1/ALDO2 to 3.3 V explicitly when it switches them, and leaves ALDO3's
3.0 V alone.

**Never turn ALDO1 off.** ALDO1 is the ES7210's only supply (VDDD/VDDP via R17,
VDDA/VDDM via R34; the VCC3V3 alternatives R15/R33 are unfitted). Measured: with
ALDO1 off, the unpowered ES7210's I/O clamps SDA/SCL — presumably through its ESD
diodes into its collapsed supply — and the 2.2 kΩ pull-ups can't lift them. Scans
then "find" dozens of addresses and every real transaction fails, the PMIC's
included, so the rail can't be switched back on over I²C. The PMIC keeps its
register state through ESP32 resets; only removing USB and battery resets it to
the OTP default (ALDO1 on). The libraries therefore refuse to switch ALDO1 off, and
`board.begin()` re-asserts it.

The AXP2101's **IRQ** (UP1.38, net `AXP_IRQ` / `EXIO5`) has only RP5 10 kΩ to VCC-RTC
and reaches no GPIO. The `EXIO5` alias suggests an earlier revision routed it to an
I/O expander that this board doesn't have. Hence polling in the power library.

### The I²C bus

R23 / R49 are **2.2 kΩ** to VCC3V3, placed by the codec. There are 22 pF capacitors
from SDA and SCL to analog ground at both the ES8311 (C34/C35) and the ES7210
(C77/C78), presumably for RF/EMI. Six devices plus these caps and short traces: 2.2 kΩ
gives a rise time well inside 400 kHz Fast-mode (≈ 0.85 × 2.2 kΩ × ~60 pF ≈ 110 ns,
inferred). The FT3168's address is fixed at 0x38. The QMI8658's SA0 (pin 1) is
grounded, which SensorLib names `QMI8658_L_SLAVE_ADDRESS` = **0x6B**, matching the
"0X6B" note on the sheet.

### USB, crystal, antenna

- **USB-C (U9):** D+/D− of both rows joined, through **22 Ω** series resistors (R19/R20)
  to GPIO19/20. Test pads TP12/TP18 sit on the chip side. CC1/CC2 have **5.1 kΩ** to
  ground (R1/R2): a plain sink, 5 V only. SBU unconnected. A TVS (LTVS16H5.0ET5G) on
  VBUS.
- **40 MHz crystal** X1 (2016 package) with 15 pF loads (C10/C12). A **24 nH** inductor
  (IND2) sits in series on XTAL_P, which is Espressif's S3 recommendation for
  suppressing crystal harmonics. R14 (across the crystal) is unfitted.
- **Analog supply:** VDD3P3 (U2.2/3) is fed from VCC3V3 through IND1 (2.0 nH) as
  `P3V3`, decoupled by C20 100 nF; C16–C19 sit on the VCC3V3 side.
- **Antenna (J2):** the matching network is a series element "L1" between `LNA_IN` and
  the antenna node, a shunt "C8" at the antenna node, and unfitted L2 / C9 / C11. The
  printed values are **"L1 0.75pF"** and **"C8 15nH"**: the reference designators and
  values look swapped (a capacitor value on an inductor designator and vice versa).
  Electrically it is either series 0.75 pF / shunt 15 nH, or the transposition.
  **TODO (bench / BOM)**. Not a software concern.

### GPIO strapping

| GPIO | Strap | Load on this board | Verdict |
|---|---|---|---|
| 0 | boot mode | R8 10 kΩ up, BOOT to ground | correct |
| 3 | JTAG source (only with `STRAP_JTAG_SEL` eFuse) | R6 10 kΩ up (SD MISO) | harmless at default eFuses |
| 45 | **VDD_SPI voltage** | codec/ADC LRCK inputs + 22 pF | relies on the S3's internal pull-down → 3.3 V VDD_SPI, which the 3.3 V flash on `VDD_SPI` needs. Any pull-up here bricks boot. |
| 46 | boot mode / ROM print | R31 10 kΩ down (amp CTRL) | correct; also keeps the amplifier off through boot |

### Vibration motor

**Not fitted on the unit examined** (observed 2026-10-06: bare MOTOR pads, nothing in
the shell contacts them). The analysis below is of the driver circuit.

ALDO3 → R7 0 Ω → motor (P1/P2) → Q1 collector (MMBT3904), emitter to ground. The base
is fed from GPIO18 through **R12 4.7 kΩ**, with **R13 47 kΩ** base-emitter.

- Base current ≈ (3.3 − 0.75) / 4.7 k − 0.75 / 47 k ≈ **0.53 mA**.
- A coin motor at ~3 V typically draws 60–90 mA, which would need a forced β of
  ~110–170. The MMBT3904's h<sub>FE</sub> falls to ~30–60 above 50 mA, so **Q1 probably
  doesn't saturate**. It runs partly linear, the motor sees less than ALDO3's voltage,
  and Q1 dissipates the difference (inferred). **TODO (bench):** measure V<sub>CE</sub>
  while buzzing.
- **No flyback diode** anywhere on the motor. At switch-off the winding's current has
  nowhere to go but through Q1's C-E breakdown (40 V rated). That survives short duty,
  but it is not good practice: keep pulses short.

### IMU and wake on motion

`AMOLED206_IMU` follows the QMI8658 datasheet's sequences, cross-checked against
SensorLib (MIT), the manufacturer's choice:

- **Reset:** write 0xB0 to `RESET` (0x60), poll `RST_RESULT` (0x4D) for 0x80 (≤ 500 ms),
  then set `CTRL1` bit 6 (address auto-increment) by read-modify-write. The rest of
  `CTRL1`, including its endianness bit, keeps its reset value. That is exactly what
  SensorLib does, so the little-endian sample decoding matches the vendor's.
- **Normal mode:** `CTRL2` = accel ±4 g (range 1) at ODR code 6 (~125 Hz); `CTRL3` = gyro
  ±512 dps (range 5) at ODR code 6 (~112 Hz); `CTRL7` = 0x03. Samples are little-endian
  int16 at 0x35 (accel) and 0x3B (gyro); temperature is 0x33/0x34 as °C + 1/256.
- **Wake on Motion:** reset; `CTRL7` = 0; `CTRL2` = ±8 g with a low-power accel ODR
  (codes 12–15 = 128/21/11/3 Hz, gyro off); `CAL1_L` = threshold in mg; `CAL1_H` =
  pin/initial-level select in bits 7:6 (00 = INT1 idle low, 10 = INT1 idle high) plus
  the blanking count in bits 5:0; then the `CTRL9` command 0x08 (write WoM settings)
  with the CmdDone handshake (`STATUSINT` bit 7, then ACK 0x00); then `CTRL7` bit 0
  (accel on) and `CTRL1` bit 3 (INT1 enable). `STATUS1` bit 2 is the WoM flag, which
  clears on read.

**Why the polarity is tied to PWR.** INT1 lands on GPIO21 and PWR on GPIO10, and both
are RTC GPIOs that only `ext1` can watch together (`ext0` is taken by BOOT on GPIO0).
The S3's `ext1` has one mode for its whole pin mask: ANY_HIGH or ANY_LOW. PWR's level
is fixed by the BSS138 inverter (HIGH while held, measured; `AMOLED206_PWR_ACTIVE_LEVEL`),
so the IMU is the side that adapts: `AMOLED206_MOTION_ACTIVE_LEVEL` follows it, and
INT1 is told to idle at the opposite level. After the wake,
`esp_sleep_get_ext1_wakeup_status()` says which pin fired.

**Why re-arm right before sleeping.** The datasheet describes the WoM interrupt as a
level change from the programmed initial value. Whether the pin returns to idle on its
own, or only after a reset, wasn't settled from the documents read (**TODO (bench)**).
If it sat at the active level when the ESP32 went to sleep, `ext1` would fire at once.
Re-running the full sequence (which starts with a reset) immediately before
`deepSleep()` puts INT1 back at idle whichever behaviour the silicon has. The blanking
count then covers the settling.

**The first sample takes more than 30 ms (measured).** After `CTRL7` enables the
sensors, `STATUS0` shows no data for over 30 ms. Worse, **if the sensors are
reconfigured before that first sample lands, the accelerometer reads zero until the
next reset** — found when a sketch called `begin()` and immediately switched to
accelerometer-only mode. `begin()` therefore polls `STATUS0` (bits 0–1) for up to
300 ms before returning, and `AMOLED206_OrientationWatch::arm()` waits for bit 0
after its own reconfiguration.

## The orientation watch (ULP program)

`AMOLED206_OrientationWatch` ([developer notes](README-orientation.md)) wakes the
chip when the board is held in a taught pose. The QMI8658 has no orientation
interrupt, so the ESP32-S3's **ULP coprocessor** reads the accelerometer during deep
sleep and decides. The program is `src/amoled206_orient_ulp.c`, whose header comment
carries the same material in short form.

### Architecture

- **FSM ULP, built at run time.** The Arduino core is compiled with
  `CONFIG_ULP_COPROC_TYPE_FSM` and 512 bytes reserved
  (`CONFIG_ULP_COPROC_RESERVE_MEM`), and ships the ULP macro "assembler"
  (`esp32s3/ulp.h`: `I_WR_REG`, `I_RD_REG`, `I_ADDI`, `I_BL`, `I_WAKE`…) but no ULP
  toolchain. So the program is an array of `ulp_insn_t` filled in by C code and
  loaded with `ulp_process_macros_and_load()`. A bonus: the window thresholds are
  compiled straight into the compare instructions.
- **A small label resolver.** The S3 macro set has no `M_MOVL` (load a label's
  address into a register), which subroutine calls need for their return address.
  The builder emits branches and `MOVI R3, <return>` with placeholder targets,
  records fix-ups, and patches them once every label's address is known: absolute
  `BX`/`BXZ` targets, `MOVI` immediates, and relative offsets for the compare-branches
  (`BL`, `BG`, `BE`, `BSGE`).
- **Open-drain software I²C on RTC GPIO.** GPIO14 (SCL) and GPIO15 (SDA) are RTC
  GPIOs, handed to the RTC domain with `rtc_gpio_init()` and their OUT bits set to 0.
  Pulling a line low = setting its bit in `RTC_GPIO_ENABLE_W1TS_REG`; releasing it =
  `RTC_GPIO_ENABLE_W1TC_REG`; the board's 2.2 kΩ pull-ups make the high level.
  `RTC_GPIO_IN_REG` reads the lines. No delays are inserted: the ULP's own
  instruction time gives SCL around 250 kHz (estimated from the instruction count),
  inside the QMI8658's 400 kHz, and it has read correctly on the bench.
- **Registers.** R0 is scratch and compare input; R1 is the byte being sent or read;
  R2 is the bit counter and, at zero, the base address for loads and stores; R3 holds
  return addresses. The stage counter counts bytes in the read loop.

### One run

The ULP timer restarts the program every period (default 200 ms):

1. **Bus recovery:** release SDA, nine SCL clocks, then STOP. Frees a slave left
   mid-byte (see the history below); harmless otherwise. The loop counts R2 down to
   0, which then serves as the base register for every data word.
2. Count the run (word 122).
3. START, 0x6B+W, register 0x35 (`AX_L`); repeated START, 0x6B+R. A NACK on the
   address ends the run.
4. Read six bytes, ACKing five and NACKing the last (the READ subroutine counts bytes
   in `stage_cnt`), then STOP. Keep only the **high** byte of each axis: 1/32 g at
   ±4 g, plenty for orientation.
5. *(Bench builds stored the released bus's idle levels here, in word 123 — the
   diagnostic that found the stuck-SDA problem. Removed to make room; see step 13.)*
6. Per axis, one modular compare: `(raw − start) & 0xFF <= width`. That is four
   instructions instead of five, and it handles windows that straddle zero.
7. All in the window: if **exit-first** is still pending (word 121 nonzero), do
   nothing. Otherwise count++, and at `dwell` wait for `RTC_CNTL_RDY_FOR_WAKEUP`,
   then `WAKE`. Any axis out: count = 0 and word 121 cleared.
8. `HALT`.

### Memory and size

128 words hold both program and data. Data is packed into the last seven:

| Word | Contents |
|---|---|
| 121 | **exit-first** flag: set to 1 by `arm(true)`, cleared by the ULP on any out-of-window sample; while set, in-window samples don't count |
| 122 | runs completed |
| 123 | unused (was the idle SCL/SDA diagnostic; removed to make room) |
| 124–126 | last AX, AY, AZ high bytes (two's complement) |
| 127 | consecutive in-window samples |

So the program must fit in 121 words. With all three axes windowed and exit-first on
it is just under that. To get there: ACK/NACK folded into the READ subroutine, high
bytes only, the four-instruction axis test, no NACK error counter, no idle-level
diagnostic, and no `MOVI R2, 0` (the recovery loop already leaves R2 at 0).
`AMOLED206_OrientationWatch::programWords()` reports the size the last `arm()`
built, or a negative number if it didn't fit. A NACK counter can't see the
failure that mattered anyway: a dead SDA line reads as an ACK. Debug-only builds had
to drop axes to make room for diagnostics.

### How it got here: the debugging history

In order, as it happened on the bench (2026-10-05/06), because several wrong turns are
instructive:

1. **The ULP's software I²C works while the CPU is awake.** Run with the CPU awake
   and printing the ULP's data words, the readings matched the normal driver exactly
   (X 26, Y 1, Z −21), with zero errors at 5 Hz.
2. **Early "successes" were an illusion.** A deep sleep with a matching window woke in
   about a second. The ULP's run counter showed about 10 runs before the wake. In
   fact the dwell count had been reached in the instant **before** deep sleep began,
   while the CPU was still awake, and the wake request was latched and fired on sleep
   entry. Genuine sleep-time reads had never been tested.
3. **Sleep-time reads were garbage.** With a pose the board was actually in, it never
   woke. A dump of the ULP's data words after a BOOT wake showed 663 runs, no errors,
   and **X 0, Y 0, Z 3**.
4. **The evidence was being overwritten.** The ULP timer keeps running after a wake.
   Until `setup()` stopped it (about 4.5 s later, after the USB-console wait) it took
   fresh — and, with the CPU awake, valid — samples. Some dumps therefore looked
   healthy. **Fix: stop the ULP as the very first thing on boot.**
   `AMOLED206_Board::begin()` now does it via `AMOLED206_OrientationWatch::reclaimBus()`.
5. **SDA stuck low.** An idle-level diagnostic (word 123) read **SCL high, SDA low**
   with the bus released. One line low and the other fine ruled out "the pads lose
   their input in sleep" for both.
6. **Pad sleep configuration, suspected and ruled out.** By default ESP-IDF switches
   pads to a sleep configuration on deep-sleep entry. `gpio_sleep_sel_dis()` was added
   for both pins; it is kept, but it didn't fix this. GPIO15 is the **XTAL_32K_P**
   pad, with its own RTC pad register (`RTC_IO_XTAL_32P_PAD_REG`) separate from
   GPIO14's `RTC_IO_TOUCH_PAD14_REG`. The ULP recorded both registers' bits 13–19
   **during** sleep: 0x41 on both (FUN_IE and MUX_SEL set, SLP_SEL clear). Not the
   pads.
7. **Floating reset lines, suspected and ruled out.** In deep sleep the touch
   controller's reset (GPIO9, no pull on the board) and the panel's reset (GPIO8)
   float. Holding both high through sleep with `gpio_hold_en()` changed nothing. The
   holds are kept anyway; the power library sets them.
8. **"Fake sleep" was fine.** The exact sleep-entry sequence — display off, ULP armed,
   wake sources configured — but with the CPU staying awake instead of calling
   `esp_deep_sleep_start()` read correctly. So the fault needed real deep sleep.
9. **SDA was stuck from the first run.** A "first bad run" marker (the run number when
   SDA was first seen low) said **run 1**. A run in progress as the chip enters deep
   sleep is cut short mid-byte; the QMI8658 is left driving a 0 bit and waiting for
   clocks that never come. That is a classic stuck I²C slave, and every later
   transaction fails: reads come back as near-zero bytes, and the ACK reads as
   "acknowledged". It is a race between the first ULP run and sleep entry, which is
   why it came and went as debug printing changed the timing.
10. **Fix: I²C bus recovery at the start of every run** — nine SCL clocks with SDA
    released, then a STOP (the standard recovery from the I²C specification).
    Harmless when nothing is stuck.
11. **`RTC_CNTL_RDY_FOR_WAKEUP` before `WAKE`.** Espressif's ULP-FSM examples poll
    `RTC_CNTL_LOW_POWER_ST_REG` bit 19 before waking the chip, so this program does
    too. It also stops the pre-sleep latching in step 2.
12. **Result.** 10 of 10 automated sleep/wake cycles woke on the pose with correct
    data (≈1 s to USB re-enumeration). The real test — the board on its side asleep,
    then raised into the taught glance pose — woke correctly (sample X 11, Y 1,
    Z −28, after 42 sleeping runs). With a window the board isn't in, it slept
    through more than 75 runs with correct readings.

13. **Exit-first, and the size squeeze it caused (2026-10-06).** A watch lying
    face-up on a table is close to the glance pose, so it would wake, time out,
    sleep and wake again. The fix (a user request) lives in the ULP program, so it
    costs no CPU wakes: `arm(true)` sets data word 121, the ULP ignores in-window
    samples while it is set, and the first out-of-window sample clears it. That is 3
    instructions: a load and a branch at the top of the in-window path, and one
    store on the out-of-window path. The first bench run with exit-first and a full
    three-axis window then reported "orientation watch failed to arm": the program
    no longer fit in 121 words. The idle-level diagnostic (2 words) and two `MOVI
    R2, 0` (the recovery loop already counts R2 down to 0) were removed to make
    room, and `programWords()` was added so the size can be checked.

Measured cost (PPK2 in series with the cell): **+80 µA** over the deep-sleep floor at
5 Hz. The IMU in low-power 21 Hz accelerometer mode is part of that.

## Sound

### ES8311 (U1)

- Address 0x18: CE (pin 20) to analog ground through R50 10 kΩ.
- Supplies: **PVDD and DVDD from VCC3V3** via R51 0 Ω (R18 from A3V3 unfitted).
  **AVDD from A3V3** via R29 0 Ω (R22 from VCC3V3 unfitted). It answers I²C at 0x18
  (measured). (Its analog would go down with ALDO1, but ALDO1 must stay on — see the
  AXP2101 section.)
- ADCVREF/DACVREF 1 µF each, VMID 1 µF.
- **MIC1P/MIC1N (pins 18/17) and ASDOUT (pin 7) are unconnected**: the ES8311 is a
  DAC only here.
- OUTP/OUTN leave through 1 µF series capacitors (C46/C44) with 22 pF to ground (C50/C51).

### ES7210 (U8)

- Address 0x40: AD0/AD1 to analog ground through R43/R42 0 Ω (R36/R35 to A3V3
  unfitted).
- Supplies: **VDDD/VDDP via R17 0 Ω from A3V3** (R15 from VCC3V3 unfitted);
  **VDDA/VDDM via R34 0 Ω from A3V3** (R33 from VCC3V3 unfitted). So the whole chip
  is on ALDO1.
- SDOUT1 → **R47 51 Ω** → GPIO42. SDOUT2 → R48 (unfitted) → the same line, the TDM
  alternative.
- INT (pin 13) → TP21, DMIC_CLK (pin 14) → TP20. Both are test pads only.
- MICBIAS12 powers both microphones. MICBIAS34 is decoupled only.
- Analog ground joins digital ground at one point, R32 0 Ω.

### The echo-reference network (MIC3)

Each side from the codec's OUTP/OUTN: **R37/R44 2.2 kΩ** → C79 **10 nF** across the
pair → **C75/C82 0.47 µF** series → **R38/R45 10 kΩ** → C81 **2.2 nF** across → **R39/R46
20 kΩ** → **R40 4.3 kΩ** across the pair (R41 "NC/3.3k" in parallel, unfitted) and C80
**100 pF** across → **C76/C83 0.22 µF** → ADC_MIC3_P/N.

- **DC attenuation**, differential: 4.3 k / (4.3 k + 2 × 32.2 k) ≈ **0.063 (−24 dB)**,
  ignoring the ES7210's input impedance.
- **Low-pass:** two differential poles, ~3.6 kHz (2 × 2.2 k with 10 nF) and ~3 kHz
  (2 × 12.2 k with 2.2 nF), plus a far-off pole from C80. That's a deliberate
  band-limit to voice frequencies for AEC.
- **High-pass** from the series capacitors: on the order of 10 Hz.

All approximate, with loading ignored.

### NS4150B (U4)

VDD from VCC3V3 (C55/C56/C57: 10 µF / 100 nF / 1 µF). CTRL from GPIO46 via R30 0 Ω,
R31 10 kΩ to ground. Bypass C61 1 µF. Inputs: OUTP/OUTN → C40/C45 0.1 µF → R52/R53
150 kΩ → IN+/IN−, each node with 22 pF to ground (C41/C47) on the codec side. The
input high-pass is 1 / (2π × 150 k × 0.1 µ) ≈ **10.6 Hz**. The gain assumes the usual
TPA2005D1-style A<sub>v</sub> = 2 × R<sub>f</sub> / R<sub>i</sub> with an internal
R<sub>f</sub> of ~150 kΩ, which gives **~2 V/V** — **TODO (datasheet)**. Outputs go
through L4/L5 0 Ω to the speaker pads P5/P6; C39/C42/C43 (filter to analog ground)
are unfitted.

### Microphones

MIC1/MIC2 are 4-pin analog MEMS parts (VDD, GND, GND, DAT). VDD comes from MICBIAS12
with 100 nF + 2.2 µF. DAT goes through L6/L7 0 Ω, then C53/C32 1 µF into MIC1_P / MIC2_P.
The N inputs go through C66/C85 1 µF to analog ground. Several filter caps (C59, C60,
C65, C67, C68, C72, C73, C84, C86, C87) are unfitted.

## TF / microSD slot

J4 in SPI wiring: D3/CS = GPIO17, CMD = GPIO1, CLK = GPIO2, D0 = GPIO3, each with a
**10 kΩ** pull-up (R3–R6) to VCC3V3. D1/D2 are no-connect. The socket's card-detect
contact and its shield go to ground, so there is no detect line. Decoupling: C3 100 nF
+ C6 2.2 µF "close to SD1". The same four lines serve 1-bit SDMMC, which is how the
manufacturer's example uses them; pull-ups on CMD and D0 are what SD mode needs
anyway.

## Schematic quirks worth knowing

- **`ADC_MIC4_N` / `ADC_MIC4_P`** are drawn on the wires *below* C93/C94, which run
  into the analog-ground bar. The labels therefore sit on AGND, and the ES7210's MIC4
  inputs are AC-coupled to ground: an unused channel, not a missing microphone.
- **`MIPI_CLKP/N`, `MIPI_D0P/N`** label J3 pins 16–28 (even), which are all tied
  together and to ground. The panel's MIPI pins are unused because it runs over QSPI.
- **`EXIO5`** on the AXP2101 IRQ net, and **`DSI_PWR_EN`**, are names inherited from a
  related design. See the display library's engineering notes for the panel side.
- The **"Parameter Set"** rail table doesn't match the wiring (see above).
- **L1/C8** values look transposed (see Antenna).

## What was not derived

- Speaker impedance, motor current (no motor fitted on the unit examined), and an
  analog trace of the PWRON node: **TODO (bench)**. (ALDO3's OTP voltage, 3.0 V, GPIO10's logic levels and the flash size
  were measured on 2026-10-05.)
- The NS4150B gain equation: **TODO (datasheet)**.
