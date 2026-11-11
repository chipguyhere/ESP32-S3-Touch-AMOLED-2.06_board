# ESP32-S3-Touch-AMOLED-2.06 — the schematic, described

**Source.** Waveshare's `ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0.pdf`, revision **V1.0**,
from the `Schematic/` folder of the manufacturer's GitHub package
`waveshareteam/ESP32-S3-Touch-AMOLED-2.06` (downloaded as `ESP32-S3-Touch-AMOLED-2.06-main`;
the repository is Apache-2.0). It is an **Altium Designer** export (PDF producer
`llPDFLib 3.x`, creation date 2025-07-22) of a single schematic document,
`ESP32-S3-Touch-AMOLED-2.06.SchDoc`. The PDF has three pages: page 1 is the one schematic
sheet; pages 2 and 3 are the top- and bottom-layer PCB assembly drawings, which carry only
component outlines and designators. The PDF is not shipped with this library; get it from the
package above. The silkscreen revision of a physical board has not been compared with the
sheet.

**How it was read: built by machine from vector geometry, then checked by eye.** The PDF's
bookmark outline lists every component and every pin, but, unlike most Altium exports,
it lists **no nets**. Connectivity was therefore rebuilt from the drawing itself:

- **Pins** were located from Altium's hidden marker strings (`PI<ref>0<pin>`, invisible
  text). Those markers are snapped to whole points, which is coarser than the 2.66 pt pin
  pitch, so each marker was matched to the nearest drawn pin segment.
- **Wires** were joined where an endpoint meets another wire's endpoint or interior (a
  T-junction). Crossing wires were not joined.
- **Net labels** were attached by their baseline to the wire beneath them.
- **Power ports** (GND, VCC3V3, …) were named from the text lying along the port's stem.
  Same-named labels and ports were then merged into one net.

The result was **checked block by block** against renders of the sheet at 450–600 dpi.
That check found and corrected these pins by hand:

- `C32.2` → `MIC2_P` and `C53.2` → `MIC1_P`.
- `C68` and `C87` sit between `MIC1_P` / `MIC2_P` and `AGND`.
- `CP3.2`, `CP8.2`, `CP9.2` and `CP12.1` → `GND`.
- The ES7210 address straps: `R35` and `R36` (A3V3 to `A1` / `A0`) and `R42` and `R43`
  (`A1` / `A0` to `AGND`).
- `R3.2` → `VCC3V3`. `Q1`'s base goes to the `R12`/`R13` node and its emitter to `GND`.
- On the SD socket: `J4.2` → `SDCS`, and `J4.6` and `J4.9` → `GND`.

All of these are cases where an odd pin colour, a pin touching a component body, or a
crowded power port defeated the matching. **Values and part numbers are not in the PDF's
metadata at all.** They were read from the rendered sheet by eye. A value can therefore be
misread or paired with the wrong designator where text is dense, even though connections
cannot. Judgement calls:

- The `ADC_MIC4_N` and `ADC_MIC4_P` labels sit on wires that join the `AGND` bar below
  C93/C94. The ES7210's fourth channel is AC-coupled to ground, and the labels name an
  unused input rather than a signal.
- The `MIPI_CLKP/N` and `MIPI_D0P/N` labels on the AMOLED connector sit on pins that are
  strapped together and to `GND`. The panel is driven over QSPI, and the MIPI pins are
  parked.
- A few printed values look swapped between part types: `L1` is labelled "0.75pF" and `C8`
  "15nH". They are recorded as printed.

**Conventions.**
- **TODO (schematic)** marks what the sheet doesn't settle, **TODO (bench)** what only a
  test would, and **inferred** a conclusion drawn from part numbers and values.
- Facts from elsewhere are tagged by source. "(demo)" means the manufacturer's Arduino or
  ESP-IDF examples in the same package.
- Pins are written `Ref.pin`. Net names are in code font as printed, with aliases on one
  net joined `A / B`.
- Waveshare puts both a function label and a `GPIOn` label on most signals; these are
  aliases on one net.

**This file's place.** It is the reference beneath the family's documentation. The board
library's [README-board.md](README-board.md), [README-sound.md](README-sound.md) and
[README-sdcard.md](README-sdcard.md) state what a sketch needs from it. The
`README-engineering.md` files of the board, display and power libraries interpret it and
cite it by section. This is the one file that tries to be complete rather than useful;
"what the circuit does and why" belongs in engineering.

---

## 1. Bill of materials, by function

### 1.1 Processor, flash and clocks

| Ref | Part | Role |
|---|---|---|
| U2 | ESP32-S3R8 (QFN56) | MCU with 8 MB in-package **octal** PSRAM |
| U3 | GD25Q256EYIGR | External SPI flash, 256 Mbit (**32 MB**), on the dedicated flash pins (`SPICS0`, `SPICLK`, `SPID`, `SPIQ`, `SPIWP`, `SPIHD`); powered from `VDD_SPI` |
| X1 | 40 MHz crystal (2.0×1.6) | Main clock; C10, C12 15 pF loads; IND2 24 nH ±5 % 0201 in series with `XTAL_P`; R14 (NC) across |
| IND1 | 2.0 nH | `VCC3V3` → `P3V3` (U2.2/U2.3, the RF/analog VDD3P3 pins); C20 100 nF on `P3V3` |
| C1, C2 | 1 µF, 100 nF | VDDA (U2.55/U2.56) |
| C7 | 100 nF | VDD3P3_CPU (U2.46) |
| C13 | 100 nF | VDD3P3_RTC (U2.20) |
| C16, C17, C18, C19 | 1 µF ×3, 10 µF 0402 | `VCC3V3` bulk at the module |
| C4 | 100 nF | `VDD_SPI` |
| C5 | NC | on `SPIHD` |
| R9, C15 | 10 kΩ, 100 nF | `CHIP_PU` pull-up and delay; `CHIP_PU` is also the AXP2101's `PWROK` (§1.4) |
| R8, C14 | 10 kΩ, 100 nF | GPIO0 pull-up and debounce (BOOT key, §1.5) |

No 32.768 kHz crystal on the ESP32. `XTAL_32K_P` and `XTAL_32K_N` are GPIO15/GPIO16,
used here for I²C SDA and I²S MCLK. U2 pins 36–42 (`SPICLK_N`/GPIO48, `SPICLK_P`/GPIO47,
GPIO33–GPIO37) are drawn with no-connect crosses. GPIO33–37 belong to the octal PSRAM
inside the package. GPIO47/48 are left unrouted. Pin 28 (`SPICS1`, GPIO26, the PSRAM chip
select) is unconnected on the sheet. **Flash size:** the sheet says 32 MB, and every
build configuration in the manufacturer's package sets 16 MB
(`CONFIG_ESPTOOLPY_FLASHSIZE_16MB`, demo); the sheet is right — measured 2026-10-05,
JEDEC ID 0xC84019, physical size 32 MB (bench).

### 1.2 Antenna

| Ref | Part | Role |
|---|---|---|
| J2 | Antenna (RF-50Ω symbol) | Feed at J2.1; J2.2 is drawn with no connection |
| L1 | printed "0.75pF" | Series element, antenna → `LNA_IN` |
| C8 | printed "15nH" | Shunt to GND at the antenna side |
| L2, C11 | NC | Optional shunt branch |
| C9 | NC | Shunt at `LNA_IN` |

### 1.3 USB-C

| Ref | Part | Role |
|---|---|---|
| U9 | USB-C receptacle (part number not printed) | `VBUS` on A4B9/B4A9, GND on A1B12/B1A12 and shell (pin 0); SBU A8/B8 no-connect |
| R2, R1 | 5.1 kΩ | CC1 (A5), CC2 (B5) pull-downs: a sink |
| R19, R20 | 22 Ω | Series resistors `USB_N`→`USB'_N` (GPIO19), `USB_P`→`USB'_P` (GPIO20) |
| TVS1 | LTVS16H5.0ET5G | `VBUS` clamp to GND |

There is no USB-UART bridge. The receptacle is the ESP32-S3's native USB, and both rows of
D+/D− are joined.

### 1.4 Power: AXP2101 PMIC and battery

| Ref | Part | Role |
|---|---|---|
| UP1 | AXP2101 | PMIC: power path, charger, fuel gauge, DCDC1, ALDO1–4, RTCLDO; I²C at 0x34 |
| J1 | 2-pin battery connector | J1.1 = `VBAT1` (+), J1.2 = GND; J1.0 (mounting) = GND |
| CP1 | 10 µF 0402 | `VBUS` |
| CP2 | 10 µF 0402 | `VMID` |
| RP1 | NC | `VMID` ↔ `PS` |
| CP5 | 2.2 µF | `VBAT1` (BAT, UP1.33) |
| CP3 | 2.2 µF | `PS` at VIN1 |
| CP7 | 2.2 µF | `PS` at ALDOIN |
| LP1 | 1 µH 2.5×2×1.2 3.5 A | DCDC1 inductor, `LX1` → `VCC3V3`; FB1 (UP1.21) on `VCC3V3` |
| CP4 | 22 µF 0603 | `VCC3V3` output |
| LP2 | 1 µH 2.5×2×1.2 3.5 A | SW (UP1.35) → `Vsys/DCDC5` |
| RP3 | 0.01 Ω 1 % 0603 | `Vsys/DCDC5` ↔ `PS` |
| CP11, CP12, CP6 | 22 µF 0603, NC (22 µF 0603), 100 nF | on `Vsys/DCDC5` |
| RP4 | NC | `Vsys/DCDC5` → `DCDC5-FB` (GPIO1/UP1.32) |
| RP2 | 10 kΩ, labelled "Thermistor 10K" | TS (UP1.31) to GND — a fixed resistor where an NTC would go |
| CP13 | 100 nF | `VREF` (UP1.2) |
| CP16 | 2.2 µF | ALDO1 output (`A3V3`) |
| CP8 | 2.2 µF | RTCLDO output (`VCC-RTC`) |
| CP9 | 2.2 µF | VBACKUP (`VBAT2`, UP1.27) — the only thing on that pin |
| CP10 | 1 nF | `PWRON` |
| RP7 | 510 Ω | `PWRON` → PWR key (§1.5) |
| RP5 | 10 kΩ | Pull-up of `AXP_IRQ` (UP1.38) to `VCC-RTC` |

How UP1's pins are used:

- **VBUS** (37) on `VBUS`. **BAT** (33) on `VBAT1`. **VSYS** (34), **VIN1** (23) and
  **ALDOIN** (17) are all on `PS`.
- **DCDC1** is the only DC-DC converter in use. It makes `VCC3V3`.
- **VIN2–VIN4** (24, 6, 7), **LX2–LX4** (25, 5, 8) and **FB2–FB4** (26, 4, 9) are
  unconnected, so DCDC2–4 are unused.
- **ALDO1** (18) is `A3V3`, the audio analog rail (§1.10–1.11).
- **ALDO2** (19) goes through R10 10 kΩ to `DSI_PWR_EN` (§1.6).
- **ALDO3** (16) goes through R7 0 Ω 0402 to the motor (§1.14). **ALDO4** (15) is drawn
  only as a power-port stub.
- **BLDOIN, BLDO1, BLDO2** (13, 12, 14), **CPUSLDO** (10), **DLDO1/DC1SW** (20) and
  **DLDO2/DC4SW** (11) are unconnected.
- **RTCLDO** (28) is `VCC-RTC`, the PCF85063's supply (§1.8).
- **PWRON** (30) goes to the PWR key. **PWROK** (29) is `CHIP_PU`: the PMIC holds the
  ESP32 in reset until its rails are good.
- **IRQ** (38), labelled `AXP_IRQ` / `EXIO5`, goes only to its RP5 pull-up. **No ESP32
  GPIO sees it.**
- **CHGLED** (1) has a label stub and **nothing else**: no charge LED is fitted.
- **SDA/SCK** (39/40) are on the shared I²C bus (§1.7).
- **GND1** (3) and **EPAD** (41) go to GND.

The sheet carries a parameter table: DCDC1 VCC3V3, DCDC2 0.9 V, DCDC3 1.2 V, DCDC4 1.8 V,
DCDC5 NC, VBAT1 "CHG BAT", VBAT2 "CHG RTC", ALDO1 "VL1 3.3V", ALDO2 "VL2 3.3V", ALDO3
"VCC3V", ALDO4 "VL3 1.8V", BLDO1 "VL 1.2V", BLDO2 "VL 2.8V", CPUSLDO "VCL 1.2V". It reads
like a template carried over from another design. The DCDC2–4, BLDO and CPUSLDO entries
name outputs whose pins go nowhere on this board. Treat it as the intended voltages of the
rails that are wired, not as a description of the board. A note "PS >100mil / >150mil"
gives trace-width targets.

### 1.5 Buttons

| Ref | Part | Role |
|---|---|---|
| Key1 | Tact switch "BOOT" | GPIO0 to GND; R8 10 kΩ pull-up, C14 100 nF |
| Key3 | Tact switch "PWR" | To GND from the node joining RP7 (510 Ω, to `PWRON`) and R16 (1 kΩ, to T1's gate) |
| T1 | BSS138LT1G N-MOSFET | Gate from the PWR node via R16; source GND; drain = `SYS_OUT` = GPIO10 |
| R11 | 10 kΩ | `SYS_OUT` pull-up to `VCC3V3` |
| R16 | 1 kΩ | Gate resistor |

GPIO10 sees the PWR key through an inverter. With the key released, the gate sits at
`PWRON`'s idle level and T1 conducts, so GPIO10 reads LOW. With the key pressed, the gate
is pulled to GND, T1 turns off and R11 pulls GPIO10 HIGH (inferred, and measured
2026-10-05: LOW idle, HIGH while pressed (bench)).
There is no reset key. `CHIP_PU` is driven by the AXP2101's `PWROK` together with R9/C15.

### 1.6 AMOLED panel connector

| Ref | Part | Role |
|---|---|---|
| J3 | 34-position FPC (30 contacts + 4 mounting) | CO5300 AMOLED module (panel part number not on the sheet; the controller name is from the package README) |
| C24, C25 | 100 nF, 10 µF 0402 | `VCC3V3` at J3.11 |
| C27, C28 | 10 µF 0402, 100 nF | `VCI` / `VDDIO` (= `VCC3V3`) at J3.23/J3.25 |
| R27 | 10 kΩ | `IM1` (J3.15) to `VCC3V3` |
| R28 | 10 kΩ | `IM0` (J3.17) to GND |
| R26 | NC/10 kΩ | Optional `TP_INT` pull-up to `VCC3V3` |
| R10 | 10 kΩ | `DSI_PWR_EN` (J3.21) pull-up to `ALDO2` |
| R54 | NC/10 kΩ | `DSI_PWR_EN` → TP14 |

J3 pin map:

| Pin | Net | Pin | Net |
|---|---|---|---|
| 1 | GND | 2 | GND |
| 3 | `TP_SCL` (GPIO14) | 4 | `QSPI_SCL` (GPIO11) |
| 5 | `TP_SDA` (GPIO15) | 6 | `LCD_CS` (GPIO12) |
| 7 | `TP_INT` (GPIO38) | 8 | `QSPI_SI3` (GPIO7) |
| 9 | `TP_RESET` (GPIO9) | 10 | `QSPI_SI2` (GPIO6) |
| 11 | `VCC3V3` | 12 | `QSPI_SI1` (GPIO5) |
| 13 | × no connect | 14 | `QSPI_SIO0` (GPIO4) |
| 15 | `IM1` (10 k to 3V3) | 16 | GND |
| 17 | `IM0` (10 k to GND) | 18 | `MIPI_CLKP` → GND |
| 19 | `LCD_RESET` (GPIO8) | 20 | `MIPI_CLKN` → GND |
| 21 | `DSI_PWR_EN` | 22 | → GND |
| 23 | `VCI` = `VCC3V3` | 24 | `MIPI_D0P` → GND |
| 25 | `VDDIO` = `VCC3V3` | 26 | `MIPI_D0N` → GND |
| 27 | `LCD_TE` (GPIO13) | 28 | → GND |
| 29 | `VCC3V3` | 30 | `VCC3V3` |
| 31–34 | GND (mounting) | | |

The panel's logic and I/O supplies (`VCI`, `VDDIO`) are on `VCC3V3` and cannot be switched
apart from the whole board. `DSI_PWR_EN` is the one supply-related input under PMIC
control. `IM1`=1, `IM0`=0 is the strap the panel module uses for QSPI (inferred from
the demo driving it over QSPI). The touch controller lives in the panel module; the
sheet shows only its four signals.

### 1.7 Shared I²C bus

| Ref | Part | Role |
|---|---|---|
| R23, R49 | 2.2 kΩ | `ESP32_SCL` / `ESP32_SDA` pull-ups to `VCC3V3` (drawn in the Codec block) |
| C34, C35 | 22 pF | SCL, SDA to `AGND` at the ES8311 |
| C77, C78 | 22 pF | SDA, SCL to `AGND` at the ES7210 |
| TP5, TP7 | Test pads | SCL, SDA |

One bus on GPIO14 (SCL) and GPIO15 (SDA). Its aliases are `ESP32_SCL / GPIO14 / RTC_SCL /
TP_SCL` and `ESP32_SDA / GPIO15 / RTC_SDA / TP_SDA`. Devices on it:

| Device | Address (7-bit) | Strap |
|---|---|---|
| FT3168 touch (in the panel, J3.3/J3.5) | 0x38 | — (demo) |
| AXP2101 (UP1) | 0x34 | fixed |
| PCF85063ATL (U6) | 0x51 | fixed |
| QMI8658C (U5) | 0x6B | SDO/SA0 to GND; sheet annotation "0X6B" |
| ES8311 (U1) | 0x18 | CE via R50 10 kΩ to `AGND` |
| ES7210 (U8) | 0x40 | AD0, AD1 to `AGND` (R43, R42 0 Ω; R36, R35 to A3V3 NC); sheet annotation "0x40" |

### 1.8 Real-time clock

| Ref | Part | Role |
|---|---|---|
| U6 | PCF85063ATL | RTC; VDD (10) on `VCC-RTC`; INT (4) = `RTC_INT` = GPIO39 |
| Y1 | 32.768 kHz (2.0×1.2) | OSCI (1) / OSCO (2) |
| C22, C23 | 22 pF | Crystal loads |

CLKOE (3) is drawn with **no connection and no no-connect cross**. n.c. (8) and CLKOUT (9)
are crossed. `RTC_INT` has **no pull-up on the board**, and the INT output is open-drain.
GPIO39 is not an RTC GPIO, so the RTC interrupt cannot wake the ESP32 from deep sleep.
The RTC's only supply is the AXP2101's RTCLDO. There is no coin cell, and `VBACKUP`
carries only CP9.

### 1.9 IMU

| Ref | Part | Role |
|---|---|---|
| U5 | QMI8658C | 6-axis IMU; VDD (8) and VDDIO (5) on `VCC3V3`; CS (12) high = I²C |
| C26 | 100 nF | Decoupling |
| TP15 | Test pad | `QMI_INT2` (U5.9) — its only connection |

SDO/SA0 (1) is on GND. SDx/SCx (2/3, the auxiliary master bus) are tied to `VCC3V3`.
NC (10, 11) are unconnected. INT1 (4) is `QMI_INT1` = GPIO21. The block is titled
"6/9-Axis", but only the 6-axis part is fitted.

### 1.10 Audio codec (ES8311)

| Ref | Part | Role |
|---|---|---|
| U1 | ES8311 | Mono codec; DAC drives the speaker path |
| R51 | 0 Ω | `VCC3V3` → `PVDD` / `DVDD` (U1.3, U1.4 joined) |
| R18 | NC | `A3V3` → `DVDD` alternative |
| C48, C49 | 100 nF | `PVDD`, `DVDD` |
| R29 | 0 Ω | `A3V3` → `AVDD` (U1.11) |
| R22 | NC | `VCC3V3` → `AVDD` alternative |
| C52 | 1 µF | `AVDD` |
| C36 | 1 µF | VMID (16) |
| C37, C38 | 1 µF | `ADCVREF` (15), `DACVREF` (14) |
| R50 | 10 kΩ | CE (20) to `AGND` → address 0x18 |
| C44, C46 | 1 µF | Output coupling: U1.13 → `OUTN`, U1.12 → `OUTP` |
| C50, C51 | 22 pF | U1.12, U1.13 to `AGND` |
| C62, C63, C64 | 22 pF | `I2S_MCLK`, `I2S_SCLK`, `I2S_LRCK` to `AGND` |
| R32 | 0 Ω 0402 | The single `AGND`–`GND` tie |

Pin use: CCLK (1) SCL; MCLK (2) GPIO16; SCLK (6) GPIO41; LRCK (8) GPIO45; DSDIN (9)
GPIO40; CDATA (19) SDA. **ASDOUT (7) and MIC1P/MIC1N (18/17) are unconnected.** The
codec's own ADC is unused, and the microphones go to the ES7210. DGND (5), AGND (10) and
EP (21) are on `AGND`.

### 1.11 Microphone ADC (ES7210)

| Ref | Part | Role |
|---|---|---|
| U8 | ES7210 | 4-channel audio ADC |
| R17 | 0 Ω | `A3V3` → VDDP (6) / VDDD (7) |
| R15 | NC | `VCC3V3` alternative |
| C74, C69, C70 | 1 µF, 100 nF ×2 | VDDP/VDDD |
| R34 | 0 Ω | `A3V3` → VDDA (22) / VDDM (23) |
| R33 | NC | `VCC3V3` alternative |
| C105, C106 | 1 µF ×2 | VDDA/VDDM |
| C33, C54, C58, C71, C101 | 2.2 µF, 2.2 µF, 100 nF, 100 nF, 1 µF | `ADC_MICBIAS12` (24) and at each mic |
| C102, C103 | 1 µF | REFP12 (17), REFQ12 (18) |
| C95, C96, C97, C98 | 1 µF | REFQM (25), MICBIAS34 (26), REFP34 (29), REFQ34 (30) |
| C99, C100 | 1 µF | `MIC1_N` → MIC1N (15), `MIC1_P` → MIC1P (16) |
| C104, C107 | 1 µF | `MIC2_P` → MIC2P (19), `MIC2_N` → MIC2N (20) |
| C88, C89 | 1 µF | `ADC_MIC3_N` → MIC3N (32), `ADC_MIC3_P` → MIC3P (31) |
| C93, C94 | 1 µF | MIC4N (27), MIC4P (28) → `AGND` |
| R47 | 51 Ω | SDOUT1 (11) → `I2S_ASDOUT` = GPIO42 |
| R48 | NC | SDOUT2 (12) → `I2S_ASDOUT` alternative |
| C90, C91, C92 | 22 pF | MCLK, SCLK, LRCK to `AGND` |
| R35, R42 / R36, R43 | NC, 0 Ω / NC, 0 Ω | AD1 / AD0 straps (§1.7) |
| TP21, TP20 | Test pads | INT (13), DMIC_CLK (14) |

MCLK (5) GPIO16, SCLK (9) GPIO41, LRCK (10) GPIO45 are shared with the ES8311, and both
chips use one I²S port. CDATA (3) is SDA and CCLK (4) is SCL. GNDD (8), GNDA (21) and
E_GND (33) are on `AGND`. Channel use: MIC1 and MIC2 are the two microphones, MIC3 is the
echo-reference loopback from the codec output (§1.13), and MIC4 is AC-grounded.

### 1.12 Microphones

| Ref | Part | Role |
|---|---|---|
| MIC1, MIC2 | 4-pin MEMS microphones (VDD, GND, GND, DAT; part number not printed) | Analog output (inferred: the output feeds an analog ADC input through a coupling capacitor) |
| L6, L7 | 0 Ω 0402 | DAT → coupling capacitor |
| C53, C32 | 1 µF | Coupling to `MIC1_P`, `MIC2_P` |
| C59, C60 / C72, C73 | NC | Filter positions at DAT |
| C66, C85 | 1 µF | `MIC1_N`, `MIC2_N` to `AGND` |
| C65, C67, C68 / C84, C86, C87 | NC | Further filter positions on the `_N` / `_P` lines |

Both mics are powered from `ADC_MICBIAS12`. Each is single-ended into a differential
input, with the `_N` side AC-grounded through 1 µF.

### 1.13 Speaker amplifier and echo-reference network

| Ref | Part | Role |
|---|---|---|
| U4 | NS4150B | Mono class-D speaker amplifier; VDD (6) on `VCC3V3` |
| R30 | 0 Ω | `PA_CTRL` = GPIO46 → CTRL (1) |
| R31 | 10 kΩ | CTRL to `AGND`: the amplifier is off unless driven |
| C61 | 1 µF | Bypass (2) |
| C40, C45 | 0.1 µF | `OUTP` / `OUTN` coupling |
| R52, R53 | 150 kΩ | Into IN+ (3) / IN− (4) |
| C41, C47 | 22 pF | `OUTP`, `OUTN` to `AGND` |
| C55, C56, C57 | 10 µF 0402, 100 nF, 1 µF | Amplifier supply |
| L4, L5 | 0 Ω 0402 | OUT+ (8) → P5, OUT− (5) → P6 |
| C39, C43, C42 | NC | Output filter positions (to `AGND`, and across) |
| P5, P6 | Pads | Speaker |
| R37, R44 | 2.2 kΩ | Echo-reference network input from `OUTP` / `OUTN` |
| C79 | 10 nF | Across the two legs after R37/R44 |
| C75, C82 | 0.47 µF | Series coupling |
| R38, R45 | 10 kΩ | Series |
| C81 | 2.2 nF | Across the legs |
| R39, R46 | 20 kΩ | Series |
| R40 | 4.3 kΩ | Across the legs (output divider) |
| R41 | NC / 3.3 kΩ | Optional, across the legs |
| C80 | 100 pF | Across the legs |
| C76, C83 | 0.22 µF | → `ADC_MIC3_P` / `ADC_MIC3_N` |

The block title is "PA&SPEAKER", and the sub-block is labelled "AEC". GND (7) is on
`AGND`.

### 1.14 Vibration motor

| Ref | Part | Role |
|---|---|---|
| P1, P2 | Pads | Motor + (P1, from `ALDO3` via R7 0 Ω 0402), motor − (P2) |
| Q1 | MMBT3904 NPN | Low-side switch; collector (3) P2, emitter (2) GND, base (1) |
| R12 | 4.7 kΩ | GPIO18 (`MOTOR`) → base |
| R13 | 47 kΩ | Base to GND |

No flyback diode is drawn across the motor.

### 1.15 TF / microSD socket

| Ref | Part | Role |
|---|---|---|
| J4 | microSD socket (10 positions) | Wired for **SPI mode** |
| R3, R4, R5, R6 | 10 kΩ | Pull-ups to `VCC3V3` on `SDCS`, `MOSI`, `SCK`, `MISO` |
| C3, C6 | 100 nF, 2.2 µF | Socket supply ("close to SD1") |

| J4 pin | Name | Net |
|---|---|---|
| 1 | D2 | × no connect |
| 2 | CD/D3 | `SDCS` = GPIO17 |
| 3 | CMD | `MOSI` = GPIO1 |
| 4 | VDD | `VCC3V3` |
| 5 | CLK | `SCK` = GPIO2 |
| 6 | VSS | GND |
| 7 | D0 | `MISO` = GPIO3 |
| 8 | D1 | × no connect |
| 9 | CD (card-detect switch) | GND (joined to pin 10) |
| 10 | GND (shell) | GND |

The card-detect switch is grounded, so card presence cannot be read. Nothing else shares
the socket's four lines.

### 1.16 Test pads and the logo

All of these sit in the "Parameter Set" box at the top right of the sheet:

| Pad | Net |
|---|---|
| TP3 | `VCC3V3` |
| TP4 | GND |
| TP13 | `U0TXD` (GPIO43) |
| TP6 | `U0RXD` (GPIO44) |
| TP5 | `ESP32_SCL` |
| TP7 | `ESP32_SDA` |
| TP12 | `USB'_N` |
| TP18 | `USB'_P` |
| P3 | GND (unlabelled pad) |
| P4 | `VBUS` (unlabelled pad) |

TP14 (`DSI_PWR_EN` via NC R54), TP15 (`QMI_INT2`), TP20 (ES7210 DMIC_CLK) and TP21
(ES7210 INT) are elsewhere on the sheet. `logo1` is the Waveshare logo and has no pins.

---

## 2. The complete GPIO assignment

| GPIO | Net | Use | Notes |
|---|---|---|---|
| 0 | `GPIO0` | BOOT key | Strapping; R8 10 k pull-up, C14 100 nF; RTC GPIO |
| 1 | `MOSI` | SD CMD | R4 10 k pull-up; ADC1_CH0; RTC GPIO |
| 2 | `SCK` | SD CLK | R5 10 k pull-up; ADC1_CH1 |
| 3 | `MISO` | SD D0 | R6 10 k pull-up; strapping (JTAG source, only if the eFuse selects it); ADC1_CH2 |
| 4 | `QSPI_SIO0` | AMOLED data 0 | ADC1_CH3 |
| 5 | `QSPI_SI1` | AMOLED data 1 | ADC1_CH4 |
| 6 | `QSPI_SI2` | AMOLED data 2 | ADC1_CH5 |
| 7 | `QSPI_SI3` | AMOLED data 3 | ADC1_CH6 |
| 8 | `LCD_RESET` | AMOLED reset | ADC1_CH7 |
| 9 | `TP_RESET` | Touch reset | ADC1_CH8 |
| 10 | `SYS_OUT` | PWR key, inverted by T1 | R11 10 k pull-up; ADC1_CH9; RTC GPIO (deep-sleep wake possible) |
| 11 | `QSPI_SCL` | AMOLED clock | |
| 12 | `LCD_CS` | AMOLED chip select | |
| 13 | `LCD_TE` | AMOLED tearing effect | Input from the panel |
| 14 | `ESP32_SCL` | Shared I²C SCL | R23 2.2 k pull-up |
| 15 | `ESP32_SDA` | Shared I²C SDA | R49 2.2 k pull-up; also XTAL_32K_P |
| 16 | `I2S_MCLK` | I²S MCLK (ES8311 + ES7210) | Also XTAL_32K_N |
| 17 | `SDCS` | SD CS (D3) | R3 10 k pull-up |
| 18 | `MOTOR` | Motor transistor base | R12 4.7 k / R13 47 k |
| 19 | `USB'_N` | USB D− | R19 22 Ω; TP12 |
| 20 | `USB'_P` | USB D+ | R20 22 Ω; TP18 |
| 21 | `QMI_INT1` | IMU INT1 | RTC GPIO: can wake from deep sleep |
| 26 | — | (PSRAM CS inside the package) | U2.28 unconnected |
| 33–37 | — | (octal PSRAM inside the package) | U2.38–42 no-connect |
| 38 | `TP_INT` | Touch interrupt | R26 pull-up NC |
| 39 | `RTC_INT` | RTC interrupt | No pull-up on the board; not an RTC GPIO; also MTCK |
| 40 | `I2S_DSDIN` | I²S data out → ES8311 | MTDO |
| 41 | `I2S_SCLK` | I²S bit clock | MTDI |
| 42 | `I2S_ASDOUT` | I²S data in ← ES7210 SDOUT1 | via R47 51 Ω; MTMS |
| 43 | `U0TXD` | Console UART TX | **Test pad TP13 only** |
| 44 | `U0RXD` | Console UART RX | **Test pad TP6 only** |
| 45 | `I2S_LRCK` | I²S word select | Strapping (VDD_SPI voltage); no external pull |
| 46 | `PA_CTRL` | Speaker amp enable | Strapping; R31 10 k pull-down (via R30 0 Ω) |
| 47, 48 | — | — | U2.37 / U2.36 (`SPICLK_P` / `SPICLK_N`) no-connect: **unrouted** |

**Free pins.** None of the S3's routed GPIOs is unassigned. The only pins with no on-board
function are GPIO43/44, which reach test pads TP13/TP6 and nothing else. GPIO47/48 exist
on this package but are not routed off the module pins, so they are unusable without
rework. JTAG (GPIO39–42) is fully used by the RTC interrupt and I²S.

**Strapping, ADC and RTC pins.**
- **Strapping pins** on the S3 are GPIO0, 3, 45 and 46.
  - GPIO0 has its pull-up, so boot is normal unless BOOT is held.
  - GPIO45 must be low at reset for the 3.3 V `VDD_SPI` this flash needs. It has no
    external pull; its internal pull-down holds it, and the codecs' LRCK inputs don't
    drive it.
  - GPIO46 is held low by R31. GPIO3's 10 k pull-up matters only if the JTAG-select
    eFuse is burned.
- **ADC1** covers GPIO1–10 and **ADC2** GPIO11–20 (S3 datasheet). Every one of them has a
  digital function here, so no channel is free.
- **RTC GPIOs** (deep-sleep wake via ext0/ext1) are GPIO0–21. Of the signals a sketch
  might wake on, BOOT (0), PWR (10) and the IMU's INT1 (21) are RTC GPIOs. The touch
  interrupt (38) and the RTC interrupt (39) are not.

**The manufacturer's own GPIO table.** The sheet has a table with columns LCD, USB, RTC,
SD, ES8311, QMI8658C, SYS and OUT. It agrees with the netlist pin for pin. Its column
placement is loose:

- GPIO10 `SYS_OUT`, GPIO17 `SDCS` and GPIO18 `MOTOR` are in the LCD column.
- GPIO39 `RTC_INT` is in the QMI8658C column, alongside the IMU's `QMI_INT1`.
- GPIO43/44 are in the OUT column.

Read the table for the GPIO→signal pairs, not for which block owns a pin.

---

## 3. Power rails

| Rail | Source | Switchable? | Feeds |
|---|---|---|---|
| `VBUS` | USB-C | No | AXP2101 VBUS; TVS1; pad P4 |
| `VBAT1` | Battery on J1 | No (the AXP2101 BATFET is internal to the PMIC) | AXP2101 BAT |
| `PS` | AXP2101 power path (VSYS) | No | DCDC1 input (VIN1), ALDO input (ALDOIN); `Vsys/DCDC5` via RP3 |
| `Vsys/DCDC5` | AXP2101 SW via LP2, and `PS` via RP3 0.01 Ω | No | Bulk caps only — **TODO (schematic):** what this node does in this configuration (see §4) |
| `VCC3V3` | AXP2101 DCDC1 | Only as "power off" | ESP32-S3, flash (via `VDD_SPI`), AMOLED logic/IO (`VCI`, `VDDIO`, J3.11/29/30), touch, IMU, ES8311 PVDD/DVDD, NS4150B, SD socket, all pull-ups |
| `P3V3` | `VCC3V3` via IND1 2.0 nH | No | ESP32 VDD3P3 (RF) |
| `VDD_SPI` | ESP32 internal | No | U3 flash |
| `A3V3` (= ALDO1) | AXP2101 ALDO1 | **Yes** (PMIC register) | ES8311 AVDD; ES7210 VDDP/VDDD/VDDA/VDDM; ES7210 address-strap positions |
| `ALDO2` | AXP2101 ALDO2 | **Yes** | `DSI_PWR_EN` pull-up (R10 10 k) only |
| `ALDO3` | AXP2101 ALDO3 | **Yes** | Vibration motor (via R7 0 Ω) |
| `ALDO4` | AXP2101 ALDO4 | Yes | Nothing |
| `VCC-RTC` | AXP2101 RTCLDO | No | PCF85063 VDD; RP5 pull-up on `AXP_IRQ` |
| `VBAT2` | AXP2101 VBACKUP | — | CP9 only; no backup cell |
| `ADC_MICBIAS12` | ES7210 MICBIAS12 | Via ES7210 registers | MIC1, MIC2 |
| `VMID`, `VREF` | AXP2101 internal references | — | CP2, CP13 |

---

## 4. What the sheet does not say

- **Flash size.** The sheet names GD25Q256EYIGR (32 MB), while the manufacturer builds for
  16 MB. Bench, 2026-10-05: JEDEC 0xC84019, 32 MB physical — the sheet is right.
- **The AXP2101's power-on (OTP) defaults.** The sheet doesn't give them. Bench,
  2026-10-05, after removing all power: every DCDC1–4 and every LDO on (DCDC_EN 0x0F,
  LDO_EN0 0xFF, LDO_EN1 0x01); ALDO1 3.3 V, ALDO2 3.3 V, ALDO3 3.0 V, ALDO4 1.8 V, BLDO1
  1.2 V, BLDO2 2.8 V, DCDC1 3.3 V; PWR on-press 128 ms, hard-off hold 6 s (0x27 = 0x14);
  charge 200 mA (0x62 = 0x08) to 4.2 V (0x64 = 0x03); VSYS power-off 2.6 V.
- **ALDO1 must stay on (bench observation, 2026-10-05).** Not visible on the sheet but
  a direct consequence of it: ALDO1 (`A3V3`) is the ES7210's only supply, and with it
  off the unpowered ES7210 clamps `ESP32_SDA`/`ESP32_SCL`. Every device on the bus,
  the AXP2101 included, then stops answering, so ALDO1 cannot be switched back on
  over I²C; ESP32 resets don't help and only removing all power recovers.
- **`Vsys/DCDC5` and RP3.** LP2 sits between the AXP2101's SW pin and a node that RP3
  (0.01 Ω) joins to `PS`, and the DCDC5 feedback route (RP4) is unfitted. Whether that
  makes SW part of the power path or a converter running into `PS` is for the datasheet.
  **TODO (schematic/datasheet).**
- **The battery.** Cell type, capacity and whether it has a protection PCB are not on the
  sheet. TS is a fixed 10 kΩ, so the charger has no real temperature input. Observed
  2026-10-06: the included cell is a LiPo pouch labelled **3.7 V 400 mAh** on an
  MX1.25 2-pin plug, with a small circuit board wrapped into it — presumably a
  protection (deep-discharge cutoff) board. **TODO (bench):** its cutoff threshold.
- **PWR polarity on GPIO10.** Inferred HIGH while pressed (§1.5); confirmed on the bench
  2026-10-05.
- **Panel module.** The CO5300 controller, the 410×502 resolution and the 22-column RAM
  offset all come from the package README and demo code (demo). The FPC pin names come
  from the sheet. What `DSI_PWR_EN` enables inside the module is not shown. Bench,
  2026-10-06: with ALDO2 off (so `DSI_PWR_EN` low) the board draws about **13 mA more**
  in deep sleep (15.3 mA against 2.2 mA), far more than R10's 10 kΩ could carry — the
  module itself responds to the pin. ALDO2 is therefore kept on.
- **FT3168 address 0x38.** From the demo; the touch chip isn't on the sheet. Confirmed on
  the bench (2026-10-05); its chip-ID register 0xA0 reads 0x00 there, not the documented
  0x03.
- **PCF85063 CLKOE floating.** Pin 3 has neither a connection nor a no-connect mark.
  **TODO (datasheet):** whether it has an internal pull. If CLKOUT runs, it costs RTC
  current.
- **Microphone part number.** Not printed. "Analog" is inferred from the coupling into
  the ES7210's analog inputs.
- **USB-C receptacle and battery connector part numbers.** Not printed.
- **Vibration motor not fitted (observation, 2026-10-06).** On the unit examined the
  two P1/P2 pads (silkscreened MOTOR) are bare and nothing in the shell contacts them.
  The driver (Q1, R12, R13, R7) is populated.

---

## Appendix A. The full netlist

Built from the vector geometry as described at the top of this file. Hand corrections are
already applied. The pins in A.3 have no wire, label or power port on the sheet.

### A.1 Named nets

Every label that appears on the net, joined `A / B`. `GPIOn` labels are the ones Waveshare placed next to each signal name; they are aliases on one net, not separate nets.

| Net | Pins |
|---|---|
| `A0` | R36.2, R43.1, U8.1 |
| `A1` | R35.2, R42.1, U8.2 |
| `A3V3` / `ALDO1` | CP16.2, R17.1, R18.1, R29.1, R34.2, R35.1, R36.1, UP1.18 |
| `ADC_MIC3_N` | C83.2, C88.1 |
| `ADC_MIC3_P` | C76.2, C89.1 |
| `ADC_MIC4_N` / `ADC_MIC4_P` / `AGND` | C33.1, C34.1, C35.1, C36.1, C37.1, C38.1, C39.1, C41.2, C43.2, C47.2, C48.1, C49.1, C50.1, C51.1, C52.2, C54.1, C55.1, C56.1, C57.1, C58.1, C59.1, C60.1, C61.2, C62.1, C63.1, C64.1, C65.1, C66.1, C67.1, C68.2, C69.1, C70.1, C71.1, C72.1, C73.1, C74.1, C77.1, C78.1, C84.1, C85.1, C86.1, C87.2, C90.1, C91.1, C92.1, C93.1, C94.1, C95.1, C96.1, C97.1, C98.1, C101.2, C102.2, C103.2, C105.2, C106.2, MIC1.2, MIC1.3, MIC2.2, MIC2.3, R31.1, R32.1, R42.2, R43.2, R50.1, U1.5, U1.10, U1.21, U4.7, U8.8, U8.21, U8.33 |
| `ADC_MICBIAS12` | C33.2, C54.2, C58.2, C71.2, C101.1, MIC1.1, MIC2.1, U8.24 |
| `ADCVREF` | C37.2, U1.15 |
| `ALDO2` | R10.1, UP1.19 |
| `ALDO3` | R7.2, UP1.16 |
| `ALDO4` | UP1.15 |
| `AVDD` | C52.1, R22.2, R29.2, U1.11 |
| `AXP_IRQ` / `EXIO5` | RP5.2, UP1.38 |
| `CHGLED` | UP1.1 |
| `CHIP_PU` / `PWROK` | C15.2, R9.1, U2.4, UP1.29 |
| `Codec_CE` | R50.2, U1.20 |
| `DACVREF` | C38.2, U1.14 |
| `DCDC5-FB` | RP4.2, UP1.32 |
| `DSI_PWR_EN` | J3.21, R10.2, R54.2 |
| `DVDD` / `PVDD` | C48.2, C49.2, R18.2, R51.2, U1.3, U1.4 |
| `ESP32_SCL` / `GPIO14` / `RTC_SCL` / `TP_SCL` | C34.2, C78.2, J3.3, R23.2, TP5.1, U1.1, U2.19, U5.13, U6.7, U8.4, UP1.40 |
| `ESP32_SDA` / `GPIO15` / `RTC_SDA` / `TP_SDA` | C35.2, C77.2, J3.5, R49.2, TP7.1, U1.19, U2.21, U5.14, U6.6, U8.3, UP1.39 |
| `GND` / `MIPI_CLKN` / `MIPI_CLKP` / `MIPI_D0N` / `MIPI_D0P` | C1.1, C2.1, C3.1, C4.1, C5.2, C6.1, C7.1, C8.1, C9.1, C10.1, C11.1, C12.1, C13.1, C14.1, C15.1, C16.2, C17.2, C18.2, C19.2, C20.2, C22.1, C23.1, C24.2, C25.1, C26.1, C27.1, C28.2, CP1.1, CP2.1, CP3.2, CP4.1, CP5.1, CP6.1, CP7.1, CP8.2, CP9.2, CP10.1, CP11.1, CP12.1, CP13.2, CP16.1, J1.0, J1.2, J3.1, J3.2, J3.16, J3.18, J3.20, J3.22, J3.24, J3.26, J3.28, J3.31, J3.32, J3.33, J3.34, J4.6, J4.9, J4.10, Key1.1, Key3.1, P3.1, Q1.2, R1.1, R2.2, R13.2, R28.1, R32.2, RP2.2, T1.2, TP4.1, TVS1.2, U2.57, U3.4, U3.9, U5.1, U5.6, U5.7, U6.5, U6.11, U9.0, U9.A1B12, U9.B1A12, UP1.3, UP1.41, X1.2, X1.4 |
| `GPIO0` | C14.2, Key1.2, R8.1, U2.5 |
| `GPIO10` / `SYS_OUT` | R11.1, T1.3, U2.15 |
| `GPIO11` / `QSPI_SCL` | J3.4, U2.16 |
| `GPIO12` / `LCD_CS` | J3.6, U2.17 |
| `GPIO13` / `LCD_TE` | J3.27, U2.18 |
| `GPIO16` / `I2S_MCLK` | C62.2, C90.2, U1.2, U2.22, U8.5 |
| `GPIO17` / `SDCS` | J4.2, R3.1, U2.23 |
| `GPIO18` / `MOTOR` | R12.1, U2.24 |
| `GPIO1` / `MOSI` | J4.3, R4.1, U2.6 |
| `GPIO21` / `QMI_INT1` | U2.27, U5.4 |
| `GPIO2` / `SCK` | J4.5, R5.1, U2.7 |
| `GPIO38` / `TP_INT` | J3.7, R26.1, U2.43 |
| `GPIO39` / `RTC_INT` | U2.44, U6.4 |
| `GPIO3` / `MISO` | J4.7, R6.2, U2.8 |
| `GPIO40` / `I2S_DSDIN` | U1.9, U2.45 |
| `GPIO41` / `I2S_SCLK` | C63.2, C91.2, U1.6, U2.47, U8.9 |
| `GPIO42` / `I2S_ASDOUT` | R47.1, R48.1, U2.48 |
| `GPIO45` / `I2S_LRCK` | C64.2, C92.2, U1.8, U2.51, U8.10 |
| `GPIO46` / `PA_CTRL` | R30.2, U2.52 |
| `GPIO4` / `QSPI_SIO0` | J3.14, U2.9 |
| `GPIO5` / `QSPI_SI1` | J3.12, U2.10 |
| `GPIO6` / `QSPI_SI2` | J3.10, U2.11 |
| `GPIO7` / `QSPI_SI3` | J3.8, U2.12 |
| `GPIO8` / `LCD_RESET` | J3.19, U2.13 |
| `GPIO9` / `TP_RESET` | J3.9, U2.14 |
| `IM0` | J3.17, R28.2 |
| `IM1` | J3.15, R27.2 |
| `LNA_IN` | C9.2, L1.2, U2.1 |
| `LX1` | LP1.1, UP1.22 |
| `MIC1_N` | C65.2, C66.2, C99.2 |
| `MIC1_P` | C53.2, C67.2, C68.1, C100.2 |
| `MIC2_N` | C84.2, C85.2, C107.2 |
| `MIC2_P` | C32.2, C86.2, C87.1, C104.2 |
| `OUTN` | C44.2, C45.1, C47.1, R44.2 |
| `OUTP` | C40.1, C41.1, C46.2, R37.2 |
| `P3V3` | C20.1, IND1.2, U2.2, U2.3 |
| `PA_INL+` | R52.1, U4.3 |
| `PA_INL-` | R53.1, U4.4 |
| `PA_OUTL+` | L4.1, U4.8 |
| `PA_OUTL-` | L5.1, U4.5 |
| `PS` | CP3.1, CP7.2, RP1.1, RP3.2, UP1.17, UP1.23, UP1.34 |
| `PWRON` | CP10.2, RP7.1, UP1.30 |
| `QMI_INT2` | TP15.1, U5.9 |
| `SPICLK` | U2.33, U3.6 |
| `SPICS0` | U2.32, U3.1 |
| `SPID` | U2.35, U3.5 |
| `SPIHD` | C5.1, U2.30, U3.7 |
| `SPIQ` | U2.34, U3.2 |
| `SPIWP` | U2.31, U3.3 |
| `TS` | RP2.1, UP1.31 |
| `U0RXD` | TP6.1, U2.50 |
| `U0TXD` | TP13.1, U2.49 |
| `USB'_N` | R19.2, TP12.1, U2.25 |
| `USB'_P` | R20.2, TP18.1, U2.26 |
| `USB_N` | R19.1, U9.A7, U9.B7 |
| `USB_P` | R20.1, U9.A6, U9.B6 |
| `VBAT1` | CP5.2, J1.1, UP1.33 |
| `VBAT2` | CP9.1, UP1.27 |
| `VBUS` | CP1.2, P4.1, TVS1.1, U9.A4B9, U9.B4A9, UP1.37 |
| `VCC-RTC` | CP8.1, RP5.1, U6.10, UP1.28 |
| `VCC3V3` / `VCI` / `VDDIO` | C1.2, C2.2, C3.2, C6.2, C7.2, C13.2, C16.1, C17.1, C18.1, C19.1, C24.1, C25.2, C26.2, C27.2, C28.1, C55.2, C56.2, C57.2, CP4.2, IND1.1, J3.11, J3.23, J3.25, J3.29, J3.30, J4.4, LP1.2, R3.2, R4.2, R5.2, R6.1, R8.2, R9.2, R11.2, R15.1, R22.1, R23.1, R26.2, R27.1, R33.1, R49.1, R51.1, TP3.1, U2.20, U2.46, U2.55, U2.56, U4.6, U5.2, U5.3, U5.5, U5.8, U5.12, UP1.21 |
| `VDD_SPI` | C4.2, U2.29, U3.8 |
| `VMID` | CP2.2, RP1.2, UP1.36 |
| `VREF` | CP13.1, UP1.2 |
| `Vsys/DCDC5` | CP6.2, CP11.2, CP12.2, LP2.1, RP3.1, RP4.1 |
| `XTAL_N` | C12.2, R14.1, U2.53, X1.1 |
| `XTAL_P` | IND2.1, R14.2, U2.54 |

### A.2 Unnamed nets

Listed as the pins they join, with a descriptive name (not a sheet label).

| Description | Pins |
|---|---|
| (antenna feed: J2, matching L1/L2/C8) | C8.2, J2.1, L1.1, L2.1 |
| (40 MHz crystal, XTAL_P side after IND2) | C10.2, IND2.2, X1.3 |
| (antenna match, L2 to C11 (both NC)) | C11.2, L2.2 |
| (32.768 kHz crystal, OSCI side) | C22.2, U6.1, Y1.2 |
| (32.768 kHz crystal, OSCO side) | C23.2, U6.2, Y1.1 |
| (MIC2 output after L7, before C32) | C32.1, C73.2, L7.2 |
| (ES8311 VMID decoupling) | C36.2, U1.16 |
| (speaker +, P5) | C39.2, C42.2, L4.2, P5.1 |
| (amp input + coupling, C40 to R52) | C40.2, R52.2 |
| (speaker -, P6) | C42.1, C43.1, L5.2, P6.1 |
| (ES8311 OUTN) | C44.1, C51.2, U1.13 |
| (amp input - coupling, C45 to R53) | C45.2, R53.2 |
| (ES8311 OUTP) | C46.1, C50.2, U1.12 |
| (MIC1 output after L6, before C53) | C53.1, C60.2, L6.2 |
| (MIC1 DAT output) | C59.2, L6.1, MIC1.4 |
| (NS4150B Bypass) | C61.1, U4.2 |
| (ES7210 VDDD/VDDP (from A3V3 via R17)) | C69.2, C70.2, C74.2, R15.2, R17.2, U8.6, U8.7 |
| (MIC2 DAT output) | C72.2, L7.1, MIC2.4 |
| (AEC network, after R37 (OUTP side)) | C75.1, C79.1, R37.1 |
| (AEC network, C75 to R38) | C75.2, R38.2 |
| (AEC network output P, before C76) | C76.1, C80.2, R39.1, R40.1, R41.1 |
| (AEC network, after R44 (OUTN side)) | C79.2, C82.1, R44.1 |
| (AEC network output N, before C83) | C80.1, C83.1, R40.2, R41.2, R46.1 |
| (AEC network, R38/R39 node) | C81.1, R38.1, R39.2 |
| (AEC network, R45/R46 node) | C81.2, R45.1, R46.2 |
| (AEC network, C82 to R45) | C82.2, R45.2 |
| (ES7210 MIC3N input) | C88.2, U8.32 |
| (ES7210 MIC3P input) | C89.2, U8.31 |
| (ES7210 MIC4N input (AC-grounded)) | C93.2, U8.27 |
| (ES7210 MIC4P input (AC-grounded)) | C94.2, U8.28 |
| (ES7210 REFQM decoupling) | C95.2, U8.25 |
| (ES7210 MICBIAS34 decoupling) | C96.2, U8.26 |
| (ES7210 REFP34 decoupling) | C97.2, U8.29 |
| (ES7210 REFQ34 decoupling) | C98.2, U8.30 |
| (ES7210 MIC1N input) | C99.1, U8.15 |
| (ES7210 MIC1P input) | C100.1, U8.16 |
| (ES7210 REFP12 decoupling) | C102.1, U8.17 |
| (ES7210 REFQ12 decoupling) | C103.1, U8.18 |
| (ES7210 MIC2P input) | C104.1, U8.19 |
| (ES7210 VDDA/VDDM (from A3V3 via R34)) | C105.1, C106.1, R33.2, R34.1, U8.22, U8.23 |
| (ES7210 MIC2N input) | C107.1, U8.20 |
| (PWR button node: PWRON via RP7, T1 gate via R16) | Key3.2, R16.2, RP7.2 |
| (AXP2101 SW (DCDC5 switch node)) | LP2.2, UP1.35 |
| (motor +, from ALDO3 via R7) | P1.1, R7.1 |
| (motor -, Q1 collector) | P2.1, Q1.3 |
| (Q1 base: GPIO18 via R12, R13 to GND) | Q1.1, R12.2, R13.1 |
| (USB-C CC2 pull-down) | R1.2, U9.B5 |
| (USB-C CC1 pull-down) | R2.1, U9.A5 |
| (T1 gate) | R16.1, T1.1 |
| (NS4150B CTRL (GPIO46 via R30, R31 pull-down)) | R30.1, R31.2, U4.1 |
| (ES7210 SDOUT1) | R47.2, U8.11 |
| (ES7210 SDOUT2 (R48 NC)) | R48.2, U8.12 |
| (DSI_PWR_EN test pad TP14 via R54 (NC)) | R54.1, TP14.1 |
| (ES7210 DMIC_CLK test pad) | TP20.1, U8.14 |
| (ES7210 INT test pad) | TP21.1, U8.13 |

### A.3 Pins on no net

Pins with no wire, label or power port. Those drawn with a no-connect cross are marked ×.

J2.2, J3.13 ×, J4.1 ×, J4.8 ×, U1.7, U1.17, U1.18, U2.28, U2.36 ×, U2.37 ×, U2.38 ×, U2.39 ×, U2.40 ×, U2.41 ×, U2.42 ×, U5.10, U5.11, U6.3, U6.8 ×, U6.9 ×, U9.A8 ×, U9.B8 ×, UP1.4, UP1.5, UP1.6, UP1.7, UP1.8, UP1.9, UP1.10, UP1.11, UP1.12, UP1.13, UP1.14, UP1.20, UP1.24, UP1.25, UP1.26
