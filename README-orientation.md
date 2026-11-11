# ESP32-S3-Touch-AMOLED-2.06 — "raise to look": the orientation watch

`AMOLED206_OrientationWatch` puts the board into deep sleep and wakes it **only when
it is held in a chosen pose** — the way you hold a watch to look at it. It is part
of this (board) library and works with the power and display libraries.

Everything here was measured on one board (2026-10-05/06) unless marked otherwise.
How the underlying ULP program works, and how it was debugged into shape, is in
[README-engineering.md](README-engineering.md#the-orientation-watch-ulp-program).

## Why it exists

The QMI8658 motion sensor can wake the board on **motion**
([wake on motion](README-board.md#wake-on-motion)), but on a wrist that means every
arm swing. It has no "orientation" interrupt. So this class uses the ESP32-S3's
**ULP coprocessor** — a tiny processor that keeps running while the main CPU is in
deep sleep — to read the accelerometer a few times a second and wake the CPU only
when the gravity vector has stayed inside a window you choose.

| | Wake on motion | Orientation watch |
|---|---|---|
| Wakes on | any movement above a threshold | being *held* in a pose for ~0.4 s |
| Extra current over the deep-sleep floor | +30 µA (measured) | **+80 µA** at 5 Hz (measured) |
| Resolution | — | 1/32 g per axis |

Both are small next to the board's deep-sleep floor of about 2 mA (see the
[power library](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#power-consumption)).

## Using it

```cpp
#include <AMOLED206_Board.h>
#include <AMOLED206_IMU.h>
#include <AMOLED206_OrientationWatch.h>
#include <chipguy_AMOLED206_display.h>
#include <chipguy_AMOLED206_power.h>

AMOLED206_Board board;
AMOLED206_IMU imu(board);
AMOLED206_OrientationWatch watch(board, imu);
chipguy_AMOLED206_display display(board);

void setup() {
    board.begin();                 // FIRST: after a wake it takes the I2C bus back from the ULP
    power.begin(board);
    display.begin();
    imu.begin();

    if (power.wakeReason() == AMOLED206_Wake::Orientation) {
        // woken by the glance pose
    }
    if (!watch.load()) {           // no pose saved yet
        // ...ask the user to hold the watch where they'd look at it, then:
        watch.teach();
        watch.save();
    }
}

void goToSleep() {
    power.prepareForSleep();       // quiet the board while I2C is still ours
    display.sleep();
    watch.arm();                   // from here on the ULP owns the I2C bus (exit-first on)
    power.deepSleep();             // wakes on the pose, BOOT or PWR; never returns
}
```

**The order matters.** `arm()` hands the I²C pins to the ULP, so anything that talks
I²C — `power.prepareForSleep()`, the touch controller, the IMU — must happen first.
`power.deepSleep()` knows this: if the watch is armed it skips its own I²C work.
After the wake, **`board.begin()` must be the first thing that touches the bus**: it
stops the ULP and returns the pins. Every library's `begin()` calls it, so in
practice any normal `setup()` gets this right.

The complete version is the power library's **GlanceWake** example: first run asks
you to teach the pose (amber screen, press BOOT), then it sleeps after 5 s and wakes
green when held in the pose. For a real watch face built on it — time, date,
battery, daily NTP, drift learning — see the RTC library's
[**GlanceWatch**](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_rtc/blob/main/README.md#examples).

### Leave the pose first

A watch put down face-up on a table is close to the glance pose. Without a rule
against it, it would wake, show the time, go back to sleep — and wake again at
once. So by default **`arm()` only counts the pose after the board has been seen
outside it since arming.** Put it down, it stays asleep; pick it up and raise it, it
wakes. This is done inside the ULP program, so it costs no extra CPU wakes.
`arm(false)` turns it off: the pose then wakes the board whenever it is held, even
if it never left it.

### The calls

| Call | What it does |
|---|---|
| `teach(toleranceMg = 350, samples = 20)` | Averages the accelerometer for ~1 s and sets a window of ±`toleranceMg` around it on every axis. Hold the board still in the pose. |
| `setWindow(xLo, xHi, yLo, yHi, zLo, zHi)` | Set the window directly, in milli-g. A pair with lo > hi (e.g. `1, 0`) means "don't care" for that axis. |
| `save()` / `load()` | Keep the window, dwell and period in NVS across power cycles. |
| `setDwell(n)` | Consecutive in-window samples needed to wake (default 2). |
| `setPeriodMs(ms)` | How often the ULP samples (default 200 ms, minimum 20). |
| `arm(requireExitFirst = true)` | Configure the IMU, start the ULP, hand it the bus, enable it as a wake source. With `requireExitFirst`, the pose only counts after the board has left it (see above). |
| `AMOLED206_OrientationWatch::programWords()` | Size of the ULP program the last `arm()` built (limit 121 words), or negative if it didn't fit. Diagnostics. |
| `disarm()` | Undo `arm()` if you decide not to sleep after all. |
| `AMOLED206_OrientationWatch::wokeByOrientation()` | True if this boot was the ULP's wake (also `power.wakeReason() == AMOLED206_Wake::Orientation`). |
| `lastSample(x, y, z)` | The ULP's last reading, in milli-g. |

### Axes and the glance pose

Windows are in the **board's** axes. Measured on the bench unit: **face-up is
negative Z**. A "glance" pose — flat, tilted about 15° toward you — read about
**X +0.3 g, Y 0, Z −1.0 g**. Lying on its side it reads ±1 g on X or Y, near 0 on Z.
`teach()` saves you working this out, and the default ±350 mg window accepts roughly
±20° around the taught pose.

A good fixed pose, if you'd rather not ask the user to teach one: the face **45° up
from flat toward +X** — **X +0.707 g, Y 0, Z −0.707 g**, ±350 mg. A pose taught by
eye on the bench unit (2026-10-09) came out at X +0.73, Y +0.05, Z −0.70 g, which is
46°. The rtc library's GlanceWatch uses this fixed pose through `setWindow()`.

### Timing

With the defaults (200 ms period, dwell 2) the pose must be held for about **0.4 s**,
plus the ESP32's boot time, before your `setup()` runs. Bench result: 10 of 10
sleep/wake cycles woke on the pose in about 1 s including USB re-enumeration, and a
real "on its side, then raised to the glance pose" test woke correctly.

## Constraints

- **No I²C between `arm()` and the wake.** The ULP drives the bus pins directly.
- **It uses the whole ULP memory.** The Arduino core reserves 512 bytes for the ULP;
  the program and its data fill it. Don't combine this with another ULP program.
- **1/32 g resolution.** The ULP reads only the high byte of each axis at ±4 g. That
  is plenty for orientation, not for measuring motion.
- **The IMU is reconfigured.** `arm()` puts it in accelerometer-only low-power mode;
  call `imu.begin()` after a wake to get the gyroscope back. (`board.begin()` order
  is enough for the bus; the IMU needs its own `begin()`.)
- **The touch controller and the panel are not part of it.** Sleep them as usual
  (`display.sleep()`; the touch controller idles by itself — see the power library).

## Troubleshooting

- **It never wakes.** Check the taught pose: `lastSample()` after a BOOT wake shows
  what the ULP last saw. If it reads close to zero on all axes, the ULP isn't getting
  data from the IMU — the engineering notes describe that failure and its fix, which
  is built in; report it if you see it.
- **It wakes immediately.** The board was already in the pose when it went to sleep
  and `arm(false)` was used. Use the default `arm()` (exit-first), or put it down
  first.
- **`arm()` returns false.** Print `AMOLED206_OrientationWatch::programWords()`: a
  negative number means the program didn't fit or didn't load.
- **I²C stops working after a wake.** Something touched the bus before
  `board.begin()`. Call `board.begin()` (or any library's `begin()`) first.
- **TODO (bench):** behaviour on a real wrist (false wakes while walking), and the
  best default tolerance and dwell for a worn watch.

## Where to go next

- [README-engineering.md](README-engineering.md#the-orientation-watch-ulp-program) — how the ULP program works, and the debugging history.
- [README-board.md](README-board.md#motion-sensor-imu) — the IMU and plain wake-on-motion.
- [Power library](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md) — `prepareForSleep()`, `deepSleep()`, and the measured currents.
