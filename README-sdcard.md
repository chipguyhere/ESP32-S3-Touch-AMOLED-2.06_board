# ESP32-S3-Touch-AMOLED-2.06 — TF / microSD card slot

Technical notes for bringing up the card slot with the driver of your choice:
`SD`, `SD_MMC`, `SdFat`, or an ESP-IDF VFS mount. The board libraries don't implement
a filesystem and aren't going to; this file is what you need so you don't have to
re-derive the wiring. It was read from the schematic (`J4`) and the manufacturer's
example; nothing here has been tested on a bench yet.

## Interface and pinout

The socket's four signal lines are wired for **SPI mode**, which also covers
**1-bit SDMMC** on the same pins. D1 and D2 are not connected, so 4-bit SDMMC is
not possible.

| Card pad | SPI role | SDMMC role | Net | GPIO | Pull-up on board |
|---|---|---|---|---|---|
| CD/D3 | CS | D3 | `SDCS` | **17** | 10 kΩ (R3) |
| CMD | MOSI | CMD | `MOSI` | **1** | 10 kΩ (R4) |
| CLK | SCK | CLK | `SCK` | **2** | 10 kΩ (R5) |
| D0 | MISO | D0 | `MISO` | **3** | 10 kΩ (R6) |
| D1, D2 | — | — | — | — | not connected |
| VDD | — | — | `VCC3V3` | — | the main 3.3 V rail, not switchable |
| Card-detect switch | — | — | `GND` | — | **tied to ground: no card detect** |

All four lines have pull-ups to 3.3 V on the board, so nothing needs enabling in
software. There is 100 nF + 2.2 µF of decoupling at the socket. With no card-detect
pin, a sketch finds out whether a card is present by trying to mount it.

## Bus sharing

**Not known to be shared with anything.** The display is on its own pins (QSPI, GPIO
4–8 and 11–12), and the flash and PSRAM are on the module's dedicated pins. GPIO1, 2,
3 and 17 go only to the socket and its pull-ups.

## SPI hosts already in use

| Host | Used by | For |
|---|---|---|
| SPI0/1 | the core | flash and PSRAM |
| `SPI3_HOST` | chipguy_ESP32-S3-Touch-AMOLED-2.06_display | the AMOLED, QSPI |
| `SPI2_HOST` (FSPI, the Arduino `SPI` object) | **free** | — |
| SDMMC slot 1 | **free** | — |

The display deliberately sits on SPI3, so the Arduino `SPI` object (SPI2 on the S3)
is free for the card.

## Strapping and loaded pins

- **GPIO3** is a strapping pin, but only matters if the eFuse that selects the JTAG
  source from it has been burnt (it isn't by default). Its 10 kΩ pull-up holds the
  default-safe level, so a card being present at reset is fine.
- **GPIO17 (CS)** is pulled up, so the card stays deselected through reset and boot.
- None of the four lines doubles as anything else on this board.

## Starting points (not a supported API)

**SPI mode** with the core's `SD` library:

```cpp
#include <SPI.h>
#include <SD.h>

SPI.begin(/*sck*/ 2, /*miso*/ 3, /*mosi*/ 1, /*ss*/ 17);
if (!SD.begin(17, SPI, 20000000)) { /* no card, or it didn't mount */ }
```

**1-bit SDMMC** is what the manufacturer's `07_LVGL_SD_Test` example uses:

```cpp
#include <SD_MMC.h>

SD_MMC.setPins(/*clk*/ 2, /*cmd*/ 1, /*d0*/ 3);
if (!SD_MMC.begin("/sdcard", /*mode1bit*/ true)) { /* failed */ }
```

In SDMMC mode GPIO17 (D3) just sits at its pull-up, which is what the card needs.
SDMMC is usually faster than SPI. **TODO (bench):** compare the two and try 40 MHz in
SPI mode.

## Where the detail comes from

Pin numbers come from the schematic's SD-CARD block and its GPIO table, and match the
manufacturer's `pin_config.h` (`SDMMC_CLK 2`, `SDMMC_CMD 1`, `SDMMC_DATA 3`,
`SDMMC_CS 17`). The derivation is in
[README-engineering.md](README-engineering.md#tf--microsd-slot), and the raw
connectivity is in [schematic-description.md](schematic-description.md).
