#pragma once

// AMOLED206_OrientationWatch - wake the ESP32-S3 from deep sleep when the
// board is held in a chosen orientation ("raise to look"), using the ULP
// coprocessor to watch the QMI8658 while the main CPU sleeps.
//
// How it works: the ULP-FSM coprocessor drives the shared I2C bus itself
// (software I2C on GPIO14/15, which are RTC pins) a few times a second, reads
// the accelerometer, and wakes the CPU only once the gravity vector has stayed
// inside a window for `dwell` consecutive samples.  The QMI8658 has no
// orientation interrupt of its own, so this is the way to get "wake when I
// look at it" without waking for every arm swing.
//
// Measured on the bench (2026-10-06): +80 uA over the board's deep-sleep floor
// at 5 Hz; 10/10 sleep/wake cycles woke correctly in ~1 s.
//
//     AMOLED206_IMU imu(board);
//     AMOLED206_OrientationWatch watch(board, imu);
//
//     watch.teach();                 // hold the watch where you'd look at it
//     watch.save();                  // keep the pose across power cycles
//     ...
//     power.prepareForSleep();       // power library: quiet the board first
//     display.sleep();
//     watch.arm();                   // from here on the ULP owns the I2C bus
//     power.deepSleep();             // wakes on orientation, BOOT or PWR
//
// After a wake, AMOLED206_Board::begin() stops the ULP and hands the bus back
// before anything else touches it; power.wakeReason() reports
// AMOLED206_Wake::Orientation.
//
// Constraints:
//   - Between arm() and the wake, nothing may use I2C (the ULP owns the pins).
//   - The ULP program and its data use the whole of the core's 512-byte ULP
//     reservation (CONFIG_ULP_COPROC_RESERVE_MEM).  Don't combine this with
//     another ULP program.
//   - The window is coarse: 1/32 g steps (the ULP reads only the high byte of
//     each axis at +/-4 g).
//
// Copyright (c) 2026 chipguyhere
// MIT License

#include <Arduino.h>
#include "AMOLED206_Board.h"
#include "AMOLED206_IMU.h"

class AMOLED206_OrientationWatch {
public:
    AMOLED206_OrientationWatch(AMOLED206_Board &board, AMOLED206_IMU &imu)
        : _board(board), _imu(imu) {}

    // Window per axis in milli-g, board axes (face-up on this board is
    // NEGATIVE Z).  Pass a lo > hi pair (e.g. 1, 0) for "don't care".
    void setWindow(int xLoMg, int xHiMg, int yLoMg, int yHiMg, int zLoMg, int zHiMg);

    // Take the current orientation as the target: average `samples` readings
    // and set the window to +/- toleranceMg around it on each axis.  The board
    // must be held still in the pose.  Returns false if the IMU didn't answer.
    bool teach(int toleranceMg = 350, int samples = 20);

    // How many consecutive in-window samples trigger a wake, and how often the
    // ULP samples.  Defaults: 2 samples, 200 ms (so ~0.4 s held = wake).
    void setDwell(uint8_t samples) { _dwell = samples ? samples : 1; }
    void setPeriodMs(uint16_t ms) { _periodMs = ms < 20 ? 20 : ms; }

    // Persist / restore the window, dwell and period in NVS.
    bool save();
    bool load();
    bool taught() const { return _taught; }

    // Hand the I2C bus to the ULP and start watching.  Configures the IMU
    // (accelerometer only, +/-4 g, low-power 21 Hz) and enables the ULP as a
    // deep-sleep wake source.  Call immediately before power.deepSleep(); the
    // CPU must not use I2C afterwards.  Returns false if the ULP program could
    // not be built or loaded.
    //
    // requireExitFirst (default true): the pose only counts after the board
    // has been seen OUTSIDE it since arming.  A watch put down face-up -- which
    // is usually close to the glance pose -- then sleeps until it is picked
    // up and raised again, instead of waking over and over.  Pass false to
    // wake whenever the pose is held, even if it never left it.
    bool arm(bool requireExitFirst = true);

    // Stop the ULP and give the bus back to the CPU (for a sketch that armed
    // and then decided not to sleep).  AMOLED206_Board::begin() does the same
    // automatically after a wake.
    void disarm();

    // True if this boot is a wake caused by the orientation watch.
    static bool wokeByOrientation();

    // The last accelerometer sample the ULP took, in milli-g (valid after a
    // wake, or while armed).
    void lastSample(int &xMg, int &yMg, int &zMg) const;

    // Size of the ULP program the last arm() built, in words (limit 121), or a
    // negative number if it didn't fit / failed to load.  For diagnostics.
    static int programWords();

    // True between arm() and the next boot (the ULP owns the I2C bus).
    static bool armed();

    // Called by AMOLED206_Board::begin(): if a previous boot armed the watch,
    // stop the ULP and return GPIO14/15 to the digital I2C peripheral.
    static void reclaimBus();

private:
    AMOLED206_Board &_board;
    AMOLED206_IMU &_imu;
    int _lo[3] = { 0, 0, 0 }, _hi[3] = { 255, 255, 255 };   // offset-binary high bytes
    uint8_t _dwell = 2;
    uint16_t _periodMs = 200;
    bool _taught = false;
};
