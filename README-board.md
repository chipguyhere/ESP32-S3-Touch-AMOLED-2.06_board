# ESP32-S3-Touch-AMOLED-2.06 — the board

What is on the board, which pin it is on, and what a sketch can do with it. This is
for a developer with a job to do. The derivations are in
[README-engineering.md](README-engineering.md), and the raw connectivity is in
[schematic-description.md](schematic-description.md). Everything here was read from
the manufacturer's schematic (V1.0) and example code, and checked on one board on
2026-10-05 (USB power, no battery): facts marked **measured** come from that run.
`**TODO (bench):**` marks what only a test would still settle.

## The module

The chip is an **ESP32-S3R8**: a dual-core ESP32-S3 with **8 MB of octal PSRAM** in the
package. Use **PSRAM = OPI PSRAM**. The display library refuses to build with "QSPI
PSRAM", which cannot talk to an octal chip.

**Flash.** A **GD25Q256**, **32 MB** — measured: JEDEC ID `0xC84019`, physical size
32 MB. Use **Flash Size = 32MB** and a 32 MB partition scheme (e.g. "32M Flash
(4.8MB APP/22MB FATFS)"). The manufacturer's own builds use 16 MB; that works too
(the bench sketches were built that way), it just leaves the top half unused.

The USB-C port is the S3's **native USB** (GPIO19/20). There is no USB-serial bridge,
so the console needs **USB CDC On Boot = Enabled**.

## The pin map

The complete table, with nets, is in
[schematic-description.md §2](schematic-description.md#2-the-complete-gpio-assignment).
Every pin below is a named constant in `AMOLED206_Board`.

| GPIO | Use | Owner | Notes |
|---|---|---|---|
| 0 | BOOT button | board | active LOW, 10 kΩ pull-up; **strapping** |
| 1 | SD card MOSI / CMD | (sketch) | 10 kΩ pull-up |
| 2 | SD card CLK | (sketch) | 10 kΩ pull-up |
| 3 | SD card MISO / D0 | (sketch) | 10 kΩ pull-up; **strapping** |
| 4–7 | display QSPI SIO0–3 | display | |
| 8 | display RESET | display | active LOW |
| 9 | touch RESET | display | active LOW |
| 10 | PWR button sense | board | via a transistor; see [Buttons](#buttons) |
| 11 | display QSPI SCLK | display | |
| 12 | display CS | display | |
| 13 | display TE (tearing effect) | — | input from the panel; unused by the library |
| 14 | I²C SCL | board | 2.2 kΩ pull-up |
| 15 | I²C SDA | board | 2.2 kΩ pull-up |
| 16 | I²S MCLK | (sketch) | to both sound chips |
| 17 | SD card CS | (sketch) | 10 kΩ pull-up |
| 18 | vibration motor | board | HIGH = on |
| 19, 20 | USB D−, D+ | core | |
| 21 | IMU interrupt 1 | — | can wake deep sleep |
| 38 | touch interrupt | display | active LOW, no pull-up on the board |
| 39 | RTC interrupt | — | open drain, **no pull-up on the board** |
| 40 | I²S DOUT (to the codec) | (sketch) | |
| 41 | I²S BCLK | (sketch) | |
| 42 | I²S DIN (from the mic ADC) | (sketch) | |
| 43 | U0TXD | — | test pad TP13 only |
| 44 | U0RXD | — | test pad TP6 only |
| 45 | I²S WS / LRCK | (sketch) | **strapping** |
| 46 | speaker amplifier enable | board | HIGH = on, 10 kΩ pull-down; **strapping** |

GPIO26 and GPIO33–37 belong to the in-package PSRAM. GPIO47 and GPIO48 are not
connected to anything, and there is no pad to reach them.

### SPI hosts

| Host | Used by | For |
|---|---|---|
| SPI0/1 | the core | flash and PSRAM |
| `SPI3_HOST` | chipguy_ESP32-S3-Touch-AMOLED-2.06_display | the AMOLED, QSPI |
| `SPI2_HOST` (FSPI, the Arduino `SPI` object) | **free** | the SD card, if you use it in SPI mode |

The display is deliberately on SPI3, so the Arduino `SPI` object stays free for the
card.

## The I2C bus

One bus, **SDA GPIO15 / SCL GPIO14**, with **2.2 kΩ pull-ups** to 3.3 V. The library
owns it as `board.Wire`, a mutex-protected wrapper around the core's `Wire`. Each
transaction locks itself, so ordinary `beginTransmission` / `write` /
`endTransmission` code is safe from several tasks. Take a
`ThreadsafeWire::Guard` only to make several transactions atomic as a group. The bus
runs at 400 kHz.

| Address | Chip | What | Notes |
|---|---|---|---|
| 0x18 | ES8311 | audio codec (speaker side) | digital side on 3.3 V; answers (measured) |
| 0x34 | AXP2101 | power management | chip ID register 0x03 reads 0x4A |
| 0x38 | FT3168 | touch controller | its chip-ID register 0xA0 reads 0x00 on the bench unit (FocalTech documents 0x03) |
| 0x40 | ES7210 | microphone ADC | **powered entirely from ALDO1** — see the warning below |
| 0x51 | PCF85063 | real-time clock | |
| 0x6B | QMI8658 | 6-axis IMU | SA0 strapped low; WHO_AM_I (reg 0x00) = 0x05 |

Measured: with ALDO1 on, a full scan finds exactly these six addresses and nothing
else.

The `has…()` probes (`hasPmic()`, `hasTouch()`, `hasRtc()`, `hasImu()`,
`hasCodec()`, `hasMicAdc()`) ask each chip in turn. The only access from outside is
two test pads: **TP5 = SCL** and **TP7 = SDA**.

> **Warning — never switch ALDO1 off.** Measured on the bench (2026-10-05): with the
> PMIC's ALDO1 rail off, the unpowered ES7210 clamps SDA/SCL, and **every** chip on
> the bus — PMIC, touch, RTC, IMU, codec — stops answering (scans show nonsense,
> reads fail). Because the PMIC itself is on the bus, it can no longer be told to
> turn ALDO1 back on, and an ESP32 reset doesn't help: the PMIC keeps its state.
> Only removing **all** power (USB and battery) recovers. So `board.audioPower(false)`
> is a no-op that returns `false`, `power.setRail(ALDO1, false)` refuses, and
> `board.begin()` turns ALDO1 on — and prints a diagnosis if the PMIC doesn't answer.

The PMIC's interrupt output is **not** wired to the ESP32. The power library polls its
status registers instead.

## Buttons

**BOOT** — GPIO0, active LOW, with a 10 kΩ pull-up and a 100 nF debounce capacitor
on the board. Read it with `board.bootButtonPressed()`. Holding it through a
power-up or reset enters the ROM bootloader. That is the recovery path for a board
that won't take an upload.

Where they are: the two side buttons. With the screen's text upright in portrait,
**BOOT is the upper one and PWR the lower one** (measured).

**PWR** — goes to the AXP2101's power-on input. In hardware:

- a press while the board is off **turns it on** (a 128 ms press is enough — the
  PMIC's power-on default, measured);
- **holding it for 6 s cuts power to everything** (PMIC default after a full power
  loss, measured), whatever the sketch is doing. The power library can change the
  hold time or disable this (`power.setPowerKeyTimes()`,
  `power.setPowerKeyHardOff()`). The factory demo firmware sets 4 s; that setting
  doesn't survive removing all power.

The same button also drives a small MOSFET (BSS138) that pulls **GPIO10** low while
the button is *up*. So **GPIO10 reads HIGH while PWR is held** and LOW otherwise —
inferred from the schematic and measured. `board.pwrButtonPressed()` uses that.
(The `AMOLED206_PWR_ACTIVE_LEVEL` macro remains in case another revision differs.)

The PMIC also latches presses in its own registers; `power.pollPowerKey()` reads
them. Measured: a **short** press is reported when the button is released; a
**long** press is reported about **1.5 s into the hold, while still held**, and that
press then reports no short press on release. So exactly one event per press.

GPIO0 and GPIO10 are both RTC pads, so either button can wake the chip from deep
sleep. The power library's `deepSleep()` arms both.

**No reset button.** The chip's EN pin is driven by the PMIC's power-good output.

## Vibration motor

**On the unit examined, no motor is fitted** (observed, 2026-10-06): the two pads
silkscreened MOTOR are bare and nothing in the shell touches them. The driver
circuit is on the board, so a motor soldered to those pads should work; the calls
below drive the circuit either way.

The circuit: a motor between the PMIC's **ALDO3** output and an NPN transistor
(MMBT3904) switched by **GPIO18** (4.7 kΩ base resistor, 47 kΩ pull-down: HIGH = on).

```cpp
board.vibrate(true);   // turns ALDO3 on if it isn't, then GPIO18 HIGH
delay(100);
board.vibrate(false);
```

`board.motorPower(false)` turns ALDO3 off for good measure when the motor isn't in
use. ALDO3's voltage is left where the PMIC's factory settings put it: **3.0 V**
(measured power-on default). The power library turns ALDO3 off in
`power.prepareForSleep()`; `board.vibrate(true)` turns it back on.

**There is no flyback diode across the motor.** Each switch-off makes a voltage spike
across the transistor. Keep pulses short and infrequent, and don't PWM it hard.
**TODO (bench):** look at the collector on a scope at switch-off.

## Real-time clock

A **PCF85063** at **0x51**, with a 32.768 kHz crystal.

- **Power:** from the AXP2101's **RTCLDO** (`VCC-RTC`). That supply stays up whenever
  a battery or USB is connected, **even with the board switched off** by PWR or
  `power.powerOff()`. So the clock keeps time across power-offs, but **not** across a
  battery disconnect with USB absent. There is no coin cell (the PMIC's backup-battery
  input has only a capacitor on it).
- **Interrupt:** the RTC's INT pin goes to **GPIO39**, which has **no pull-up on the
  board**. Use `INPUT_PULLUP`. GPIO39 is **not an RTC GPIO**, so an RTC alarm **cannot
  wake the ESP32 from deep sleep**. Use the ESP32's own timer for that, or keep the
  chip in light sleep, where any GPIO can wake it. An alarm also cannot turn a
  powered-off board back on: the INT line doesn't reach the PMIC.
- **CLKOUT** is unconnected.

Measured (2026-10-05): it answers at 0x51; on a board that has never been set the
oscillator-stop flag is set (the support layer reports "time unknown") and CTRL1/2
read 0x00; INT idles high with the internal pull-up.

The driver is the sibling library
**[chipguy_ESP32-S3-Touch-AMOLED-2.06_rtc](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_rtc/blob/main/README.md)**:
`AMOLED206_RTC rtc(board);` then `rtcSupport.begin(&rtc);` (it plugs into
chipguy_RealTimeClockSupport). It is a board fork of the generic
chipguy_RTC_PCF85063 whose every transaction goes through `board.Wire`, so clock
reads are safe alongside touch, PMIC and IMU traffic in other tasks. Use the same
`board` object as everything else — with the display library, the one `lv_setup.hpp`
creates. Its `begin()` enables the internal pull-up on GPIO39. Don't use the generic
driver with `board.Wire.raw()`: that bypasses the bus lock.

## Motion sensor (IMU)

A **QMI8658** 6-axis accelerometer and gyroscope at **0x6B**, in I²C mode, on 3.3 V
(always powered, including in deep sleep — which is what lets it watch for motion).

- **INT1 → GPIO21**, an RTC GPIO: it can wake the ESP32-S3 from deep sleep.
- **INT2** goes only to test pad **TP15**.

Measured (2026-10-05): WHO_AM_I correct; at rest |a| = 1.013 g, gyro within ±2 dps,
die temperature ~35 °C.

This library includes a small driver, `AMOLED206_IMU` (`#include <AMOLED206_IMU.h>`),
on the shared bus:

```cpp
AMOLED206_IMU imu(board);
imu.begin();                       // accel ±4 g, gyro ±512 dps, both ~125 Hz
float x, y, z;
imu.readAccel(x, y, z);            // g
imu.readGyro(x, y, z);             // degrees/second
float t = imu.readTemperature();   // °C
imu.sleep();                       // both sensors off; begin() to restart
```

### Wake on motion

```cpp
imu.enableWakeOnMotion();          // arm, immediately before sleeping
power.deepSleep(0, true);          // power library: wake on BOOT, PWR or motion
// after the reboot: power.wakeReason() == AMOLED206_Wake::Motion
```

`enableWakeOnMotion(thresholdMg = 120, blankingSamples = 32, lowPowerHz = 21)`
resets the chip into its Wake-on-Motion mode: accelerometer only, at a low-power
rate (128, 21, 11 or 3 Hz — lower draws less and notices later), no data output.
Movement above `thresholdMg` (1–255 milli-g) drives INT1 to its active level.
`blankingSamples` (0–63) are ignored right after arming, to skip the jolt of putting
the board down. Around 60 mg catches a tap; 200 mg needs a shake.

- **Re-arm right before every sleep.** Arming resets INT1 to its idle level, so an
  old event can't wake the board the moment it goes to sleep.
- **After waking, call `begin()`** to get readings again: WoM mode outputs no data.
- **INT1's active level follows `AMOLED206_PWR_ACTIVE_LEVEL`** (HIGH unless you
  changed it). The S3's `ext1` wake has one level setting for all its pins, and the
  PWR button on GPIO10 is on `ext1` too, so the IMU is configured to match.
- `imu.motionDetected()` reads and clears the WoM flag, for use while awake.
- **TODO (bench):** WoM current at each low-power rate; whether INT1 holds its
  active level after an event or toggles back (the driver re-arms before each sleep
  either way); sensible default threshold on a real watch.

The register sequence is in [README-engineering.md](README-engineering.md#imu-and-wake-on-motion).

### Waking on orientation instead

Wake-on-motion wakes the board on **any** movement — every arm swing, on a watch.
The QMI8658 has no "orientation" interrupt, so for "wake when I hold it up to look"
this library adds `AMOLED206_OrientationWatch`: the ESP32-S3's ULP coprocessor reads
the accelerometer during deep sleep and wakes the CPU only when the board is in a
taught pose. Measured cost: **+80 µA** over the deep-sleep floor (WoM: +30 µA).
Everything about it is in **[README-orientation.md](README-orientation.md)**.

## Peripheral power switches this library owns

The PMIC (AXP2101) has several programmable outputs. Three of them feed board
peripherals and nothing else, so this library switches them (raw access:
`board.pmicRead/Write/Update`). The full rail table is in
[schematic-description.md §3](schematic-description.md#3-power-rails), and battery,
charging and power-off are in the
[power library](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#power-characteristics-at-a-glance).

| Call | Rail | Feeds | Voltage set |
|---|---|---|---|
| `board.audioPower(on)` | ALDO1 (`A3V3`) | ES8311 analog, all of the ES7210, mic bias | 3.3 V; **on only** — `false` is refused (see [the I²C warning](#the-i2c-bus)) |
| `board.panelPower(on)` | ALDO2 | the AMOLED's power-enable pin (via 10 kΩ) | 3.3 V; **on only** — `false` is refused: the board draws ~13 mA *more* with it off (measured) |
| `board.motorPower(on)` | ALDO3 | the vibration motor | unchanged |
| `board.ampEnable(on)` | GPIO46 | the speaker amplifier's enable | — |

ALDO4 is not connected to anything. The main 3.3 V rail (DCDC1) feeds the ESP32
itself and cannot be switched without turning the board off. At power-on the PMIC
has **every** output on (measured), including the unconnected ones; the power
library's `begin()` turns those off (DCDC2–4, ALDO4, BLDO1/2, CPUSLDO, DLDO1/2 —
measured ~0.4 mA saved in sleep).

## Free pins and common hookups

The honest answer: **there are essentially no free GPIOs.** Every ESP32-S3 pin that
reaches the board edge is used, and there is no header. The only externally
reachable signals are **test pads** for soldering fine wire to:

| Pad | Signal |
|---|---|
| TP3 / TP4 | 3.3 V / GND |
| TP5 / TP7 | I²C SCL / SDA |
| TP13 / TP6 | GPIO43 (U0TXD) / GPIO44 (U0RXD) |
| TP12 / TP18 | USB D− / D+ (ESP32 side of the 22 Ω resistors) |
| TP15 | IMU INT2 (an IMU output, not a GPIO) |
| TP20 / TP21 | ES7210 DMIC_CLK / INT (sound-chip pins, not GPIOs) |
| TP14 | the panel's power-enable, through an unfitted resistor |

So for the common jobs:

- **An I²C sensor:** solder to **TP5 (SCL), TP7 (SDA), TP3, TP4** and use
  `board.Wire`. Avoid addresses 0x18, 0x34, 0x38, 0x40, 0x51 and 0x6B. The board's
  2.2 kΩ pull-ups are already there.
- **A UART device (GPS, another MCU):** `Serial1.begin(baud, SERIAL_8N1, 44, 43);`
  using **TP6 (RX, GPIO44)** and **TP13 (TX, GPIO43)**. The ROM bootloader prints on
  GPIO43 at reset, so the device must tolerate a burst of boot text.
- **A button or an extra output:** GPIO43 and GPIO44 again, if you aren't using them
  as a UART. They are ordinary GPIOs once the console is on USB.
- **An SPI device:** it can share SPI2 with the SD card (MOSI 1, CLK 2, MISO 3), but it
  needs its own chip-select pin. The only candidates are GPIO43/44. If you don't use a
  card, the slot's four lines (1, 2, 3, 17) are four GPIOs with 10 kΩ pull-ups, wired
  to the card socket's pads.

## Strapping pins

| GPIO | Strap | On this board | Consequence |
|---|---|---|---|
| 0 | boot mode | BOOT button, 10 kΩ pull-up | Holding BOOT at reset enters download mode — intended. |
| 3 | JTAG source (only if the eFuse selects it) | SD MISO, 10 kΩ pull-up | Harmless with default eFuses. A card driving D0 at reset doesn't matter either. |
| 45 | VDD_SPI voltage | I²S WS (input to both codecs) | Must be LOW at reset for 3.3 V flash. The codecs present no pull-up, and the chip's internal pull-down wins. Don't add a pull-up here. |
| 46 | boot mode / ROM log | amplifier enable, 10 kΩ pull-down | The pull-down is the safe level, and it keeps the speaker off until a sketch enables it. |

## Where to go next

- [README-orientation.md](README-orientation.md) — waking on a "raise to look" pose.
- [README-sound.md](README-sound.md) — the audio chain.
- [README-sdcard.md](README-sdcard.md) — the card slot.
- [README-limitations-errata.md](README-limitations-errata.md) — what the board can't do or does wrong.
- [README-engineering.md](README-engineering.md) — why each of the above is true.
- [Power library](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md) — battery, charging, sleep and off.
