# ESP32-S3-Touch-AMOLED-2.06 — limitations and errata

What this board can't do, does wrong, or can get stuck in, in one place. Each item
says what you'll see and what to do about it, and links to where the detail lives.
It covers the whole family: this board library, the
[display](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README.md),
[power](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md) and
[clock](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_rtc/blob/main/README.md) libraries.

## Hazards

1. **Switching off the audio rail (ALDO1) locks up the I²C bus until all power is
   removed.** The unpowered microphone chip clamps SDA/SCL, so every chip on the
   bus stops answering — the power chip included, so it can't be told to turn ALDO1
   back on, and resetting the ESP32 doesn't help. The libraries refuse to do it
   (`board.audioPower(false)` and `power.setRail(ALDO1, false)` are no-ops); don't
   write the PMIC register yourself. Recovery: unplug USB and the battery.
   Measured. See [README-power.md, peripheral power switches](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#peripheral-power-switches).

## Display

2. **The display controller garbles partial-width writes.** A write window
   narrower than the panel leaves a faint vertical line, 2–4 px wide, at its left
   edge on light backgrounds, and its pixels change with each redraw. Aligning the
   window's start to 4 or 8 columns doesn't help; only full-width rows do.
   `lv_setup` handles it by keeping a copy of the screen in PSRAM and sending whole
   rows, so LVGL sketches need do nothing. Sketches that call
   `display.flushRegion()` directly with a narrow window will show it. Measured on
   two boards. See the display library's
   [README-engineering.md](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README-engineering.md#partial-width-writes-leave-a-line).
3. **Without PSRAM, landscape still shows that line.** If the board options turn
   PSRAM off, `lv_setup` can only stretch LVGL's bands to full width, and in the
   landscape rotations those bands are narrow strips of the panel's rows. Select
   **PSRAM: OPI PSRAM**; the build refuses "QSPI PSRAM", which can't talk to this
   chip. See [the display README's board settings](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README.md#arduino-ide-board-settings).
4. **The glass has rounded corners that hide part of the pixel grid.** Each corner
   is cut on a circle of radius about 115 px, so anything placed in the corners of
   the 410×502 grid is masked. Keep UI inside a rounded rectangle of that radius
   (`AMOLED206_CORNER_RADIUS`). Measured on one board. See
   [the display README](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README.md#the-screen-has-rounded-corners).
5. **The panel forgets its picture in its deepest sleep.** `display.sleep()` uses
   deep standby, which loses the frame memory, and only a hardware reset wakes it.
   `display.wake()` / `lv_setup.wake()` re-initialize it and send the picture back,
   so there is a brief re-init on every wake. See the display library's
   [README-engineering.md](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README-engineering.md#dsi_pwr_en-and-what-sleep-does).

## Power and sleep

6. **The board can't power itself off and come back on a timer.** With the rails
   off, only PWR (or plugging in USB) turns it on: the power chip has no wake
   timer, and the clock's alarm reaches only the ESP32. For a timed wake, use ESP32
   deep sleep (`power.deepSleep(seconds)`), which keeps the rails up. See
   [README-power.md, shutting down, sleeping, waking](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#shutting-down-sleeping-waking).
7. **Deep sleep draws about 2 mA, and part of that is unexplained.** With
   everything the libraries can switch off switched off, the floor is 1.9–2.4 mA:
   about 7–8 days on the included 400 mAh cell. Powered off it is 52 µA. **TODO
   (bench):** what keeps the rest of the floor up. See
   [README-power.md, power consumption](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#power-consumption).
8. **The touch controller can't be put to sleep.** For about 14 s after every
   sleep entry it draws about 1.3 mA extra, and it ignores its hibernate command.
   Holding it in reset stops that but costs 0.28 mA all the time, so for sleeps
   longer than about a minute the libraries let it idle. Touch also can't wake the
   board from deep sleep (wrong pin). Measured. See the display library's
   [touch and sleep](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_display/blob/main/README-engineering.md#touch-and-sleep).
9. **The low-battery cutoff is abrupt, and there is no warning signal.** The power
   chip cuts power when the system voltage falls to its threshold (2.6 V by
   default) with no notice to the sketch. Its interrupt line isn't connected to the
   ESP32, so its low-battery warning can only be found by polling its registers.
   The sketch owns the graceful shutdown: watch `getBatteryVoltage()` and save
   state around 3.4 V. See
   [README-power.md, power management chip](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#power-management-chip-and-protection).
10. **No power or charge light.** The screen is the only sign the board is on; the
    power chip's charge-LED pin isn't connected. See
    [README-power.md, indicators](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#indicators).
11. **The USB serial port disappears while the board sleeps.** It is the ESP32-S3's
    own USB, which goes down with the CPU. To upload or open the Serial Monitor,
    wake the board first (a press of BOOT or PWR), or have the sketch stay awake on
    USB power. Each wake can also come back on a different `/dev/cu.usbmodem…`
    name on a Mac.
12. **The clock has no coin cell.** It keeps time through power-off only while the
    battery or USB is connected; unplug both and the time is lost. See
    [the clock library](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_rtc/blob/main/README.md#what-to-know-about-this-clock).

## Buttons

13. **Holding PWR turns the board off in hardware, whatever the sketch is doing.**
    The default is 6 s after a full power loss (the factory firmware sets 4 s).
    Use PWR for short presses only. It can be lengthened or disabled
    (`power.setPowerKeyTimes()`, `setPowerKeyHardOff()`), but that lasts only until
    the power chip loses all power. See
    [README-power.md, the power button](https://github.com/chipguyhere/ESP32-S3-Touch-AMOLED-2.06_power/blob/main/README.md#the-power-button).
14. **One board's PWR reading shows presses that didn't happen.** On one unit
    (out of several), GPIO10 reads "pressed" in bursts of 1–2 ms pulses, a few ms
    apart, with nobody touching it, lying on a desk, at any charge (first noticed
    at 9–12%, then again at 85–90%). Each burst is enough to wake the board from
    sleep through PWR, to put up a "hold to power off" box, or to count as a press
    in a sketch. The power chip doesn't register them; its own input debounces
    pulses that short. Other units with the same firmware don't do it, so it looks
    like a fault on that board: an intermittent contact or leak at the PWR switch
    or its transistor. **Workaround:** count PWR only when GPIO10 stays high for
    30 ms or more (a real press lasts 100 ms or more), and after a PWR wake check
    that before turning anything on. `power.pollPowerKey()` reports only presses
    the power chip saw. **TODO (bench):** the fault itself. The tests so far, and
    the workaround's code, are in the `watch-calorie-log` sketch
    (`README-telemetry.md`). See
    [the PWR button path](README-engineering.md#the-pwr-button-path).
15. **No reset button.** The ESP32's reset line is driven by the power chip.
    Holding PWR until the screen goes dark, then pressing it again, is the way to
    restart a hung board; holding BOOT through that power-up enters the
    bootloader. See [README-board.md, buttons](README-board.md#buttons).

## Pins

16. **There are essentially no free GPIOs, and no header.** Everything that
    reaches the board edge is used. The only spare signals are test pads: I²C, and
    GPIO43/44 (the UART the ROM bootloader prints on at reset). The SD slot's four
    lines are also usable if no card is needed. See
    [README-board.md, free pins](README-board.md#free-pins-and-common-hookups).
