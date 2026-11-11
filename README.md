# chipguy_ESP32-S3-Touch-AMOLED-2.06_board

The **board** library for the **Waveshare ESP32-S3-Touch-AMOLED-2.06**, a small
watch-sized board with a 2.06" AMOLED touch screen. It owns the one thing several
parts of this board have to share, the **I²C bus**. It also writes down, in one
place, **what is on the board and which pin it is on**. That way the other libraries
and your sketch can use names instead of numbers.

It is one of four libraries for the board:

| Library | What it does |
|---|---|
| [chipguy_ESP32-S3-Touch-AMOLED-2.06_display](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README.md) | The screen, touch, and the LVGL glue. **Start there** if you want to put something on the screen. |
| **chipguy_ESP32-S3-Touch-AMOLED-2.06_board** (this one) | The shared I²C bus, the pin map, the buttons, the motion sensor and "raise to look" wake, the vibration motor output, and the power switches for sound and the screen |
| [chipguy_ESP32-S3-Touch-AMOLED-2.06_power](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md) | The battery, charging, USB detection, the power button, sleep and power-off |
| [chipguy_ESP32-S3-Touch-AMOLED-2.06_rtc](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_rtc/blob/main/README.md) | The real-time clock: keeps the date and time, even while the board is off |

The display and power libraries need this one installed. You will rarely call it
directly. The parts a sketch is most likely to want are the buttons, the motion
sensor, and the "raise to look" wake.

---

## Installation

Download the ZIP from GitHub (**Code ▸ Download ZIP**), unzip, and move the folder
into your Arduino `libraries` folder (`~/Documents/Arduino/libraries/` on
macOS/Linux, `Documents\Arduino\libraries\` on Windows). Restart the IDE. It needs
nothing except the ESP32 board package.

Board settings in the Arduino IDE (**Tools** menu):

| Setting | Value |
|---|---|
| Board | **ESP32S3 Dev Module** |
| PSRAM | **OPI PSRAM** |
| Flash Size | **32MB (256Mb)** (the chip is 32 MB — see [README-board.md](README-board.md#the-module)) |
| Partition Scheme | a 32 MB scheme, e.g. "32M Flash (4.8MB APP/22MB FATFS)" |
| USB CDC On Boot | **Enabled** |

The USB-C port is wired straight to the ESP32-S3 chip. There is no separate USB
serial chip, so without **USB CDC On Boot** your `Serial.print` goes nowhere.

## The one API

```cpp
#include <AMOLED206_Board.h>

AMOLED206_Board board;

void setup() {
    board.begin();                        // starts the shared I2C bus

    if (board.bootButtonPressed()) { /* BOOT is held */ }
    if (board.pwrButtonPressed())  { /* PWR is held  */ }

    board.vibrate(true);                  // buzz...
    delay(80);
    board.vibrate(false);                 // ...stop

    board.audioPower(true);               // power for the sound chips
    board.ampEnable(true);                // speaker amplifier on
}
```

If you are using the display library's examples, a `board` object already exists
(`lv_setup.hpp` creates it), so just use it.

The internal I²C bus is `board.Wire`. It works like Arduino's `Wire`, but it is safe
to use from several tasks at once. Every pin and I²C address is a named constant:
`AMOLED206_Board::SDA_PIN`, `::BOOT_BUTTON_PIN`, `::ES8311_ADDR`, and so on. The
**BoardScan** example (File ▸ Examples ▸ chipguy_ESP32-S3-Touch-AMOLED-2.06_board)
prints them all and checks which chips answer.

---

## What is on the board

A short tour. Each item has a few sentences here and a link to the file with the
detail.

### The I²C bus

One I²C bus connects everything on this board that talks I²C. That is the touch
screen controller, the power chip, the clock, the motion sensor, and the two sound
chips. The library owns it as `board.Wire`, and the other libraries use it through
that. There is no connector for it, only two tiny test pads.
**[README-board.md](README-board.md#the-i2c-bus)** has the list of chips and
addresses.

### Buttons

There are two buttons. **BOOT** is a normal button your sketch can read with
`board.bootButtonPressed()`. If you hold it while the board turns on, the chip waits
for a new program, which is how you rescue a board that won't accept an upload.

**PWR** turns the board on. Your sketch can also read it with
`board.pwrButtonPressed()`. But **holding PWR for several seconds turns the board off
in hardware**, no matter what your sketch is doing. So use PWR for short presses
only. Details are in **[README-board.md](README-board.md#buttons)**.

### Vibration motor

The board has the wiring for a small vibration motor, like the one in a phone:
`board.vibrate(true)` starts it and `board.vibrate(false)` stops it. **On the unit
we examined, no motor was fitted** — the two pads marked MOTOR were bare — so
yours may not buzz at all. See
**[README-board.md](README-board.md#vibration-motor)**.

### Speaker and microphones — [README-sound.md](README-sound.md)

The board has a speaker amplifier, a sound chip to drive it, and two microphones
with their own recording chip. These libraries do not play sound for you.
**[README-sound.md](README-sound.md)** explains how the sound parts are wired and
where to start. The two calls every sound sketch needs are `board.audioPower(true)`
and `board.ampEnable(true)`. One warning: the sound chips' power must **never** be
switched off on this board — doing so jams the connection to every other chip
(screen touch included) until you unplug all power. The library won't let you
turn it off.

### Clock and motion sensor

There is a **real-time clock** chip that keeps time while the board is switched off,
as long as the battery is connected. Its own library,
**[chipguy_ESP32-S3-Touch-AMOLED-2.06_rtc](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_rtc/blob/main/README.md)**,
reads and sets it.

There is also a **motion sensor** (an accelerometer and gyroscope) that can tell how
the board is tilted or moving. This library includes a small driver for it,
`AMOLED206_IMU`. It can also **wake the board from sleep when it is moved** — the
power library's **WakeOnMotion** example shows how. Details are in
**[README-board.md](README-board.md#motion-sensor-imu)**.

### "Raise to look" — [README-orientation.md](README-orientation.md)

A watch should wake up when you turn it to look at it, not every time your arm
swings. `AMOLED206_OrientationWatch` does that: you show it the way you hold the
watch to look at it once, and from then on the board sleeps until it is held that
way. While the main chip sleeps, a tiny helper processor inside it checks the
motion sensor a few times a second, so it costs very little battery. The power
library's **GlanceWake** example is a complete starting point, and
**[README-orientation.md](README-orientation.md)** explains how to use it.

### TF / microSD card slot — [README-sdcard.md](README-sdcard.md)

There is a microSD card slot. It does **not** share anything with the screen, so the
card and the screen can run at the same time. Pins and a copy-paste starting point
are in **[README-sdcard.md](README-sdcard.md)**.

### Battery and USB — [power library](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#power-characteristics-at-a-glance)

The board runs from USB-C or from a small lithium battery, which charges whenever
USB is plugged in. A power-management chip runs all of that, and software can talk
to it. It can read the battery, control charging, and even switch the board fully
off. The **[power library](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#power-characteristics-at-a-glance)**
covers it.

---

## Where to go next

| File | For whom |
|---|---|
| [README-board.md](README-board.md) | Developers: the full pin map, the I²C bus and its addresses, the buttons, the motor, the clock and motion sensor, the power switches this library controls, and the (very few) spare pins. |
| [README-orientation.md](README-orientation.md) | Developers: the "raise to look" wake — the call order, teaching a pose, the cost, and troubleshooting. |
| [README-sound.md](README-sound.md) | Developers who want sound out of the speaker or audio in from the microphones. |
| [README-sdcard.md](README-sdcard.md) | Developers mounting the card with the driver of their choice. |
| [README-limitations-errata.md](README-limitations-errata.md) | Anyone choosing this board or starting a feature: what it can't do or does wrong, in one short list. |
| [README-engineering.md](README-engineering.md) | Engineers: the schematic-level reasoning behind the files above, and how the orientation watch's ULP program works and was debugged. |
| [schematic-description.md](schematic-description.md) | Engineers, and LLMs without the drawing: the manufacturer's schematic narrated part by part and net by net. |
| [display library README](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README.md) | Putting something on the screen. |
| [power library README](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md) | Running on battery, and sleeping until the board is moved. |
| [rtc library README](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_rtc/blob/main/README.md) | Reading and setting the clock. |

## License

MIT — see [LICENSE](LICENSE). `chipguy_ThreadsafeWire` is a verbatim copy of the one
in `chipguy_Board`, vendored so this library has no dependencies.
