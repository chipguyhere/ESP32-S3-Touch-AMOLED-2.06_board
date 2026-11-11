#pragma once

// QMI8658 6-axis IMU on the Waveshare ESP32-S3-Touch-AMOLED-2.06.
//
// A deliberately small driver on the board's shared I2C bus: accelerometer,
// gyroscope and temperature readings, and the chip's Wake-on-Motion (WoM)
// engine wired to INT1 -> GPIO21.  GPIO21 is an RTC GPIO, so a WoM event can
// wake the ESP32-S3 from DEEP sleep (see chipguy_AMOLED206_power's
// deepSleep(seconds, wakeOnMotion)).  INT2 goes only to test pad TP15.
//
// The IMU runs from VCC3V3 (DCDC1), so it stays powered through deep sleep --
// which is what lets it watch for motion -- and cannot be power-gated; its
// own disable (sleep()) is the low-power state.
//
// Register sequences follow the QMI8658 datasheet, cross-checked against
// SensorLib (MIT), which the manufacturer's examples use.
//
// Copyright (c) 2026 chipguyhere
// MIT License

#include <Arduino.h>
#include "AMOLED206_Board.h"

// The WoM interrupt's ACTIVE level on INT1.  It has to match the ext1 wake
// mode the power library uses for the PWR button on GPIO10 (the S3's ext1
// takes one mode for all its pins), so it follows AMOLED206_PWR_ACTIVE_LEVEL.
#define AMOLED206_MOTION_ACTIVE_LEVEL AMOLED206_PWR_ACTIVE_LEVEL

class AMOLED206_IMU {
public:
    explicit AMOLED206_IMU(AMOLED206_Board &board) : _board(board) {}

    // Reset the chip, check WHO_AM_I, and start the accelerometer (+/-4 g)
    // and gyroscope (+/-512 dps) at ~125 Hz.  Returns false if it doesn't
    // answer.
    bool begin();
    bool present() const { return _ok; }

    // Latest sample.  Accel in g, gyro in degrees/second.  False on a bus
    // error or while wake-on-motion mode is armed (no data output then).
    bool readAccel(float &x, float &y, float &z);
    bool readGyro(float &x, float &y, float &z);
    float readTemperature();         // deg C, NAN on error

    // ── Wake on motion ──────────────────────────────────────────────────────
    // Reset the chip into WoM mode: accelerometer only, low-power ODR, no
    // data output, INT1 idle at the inactive level and switching to
    // AMOLED206_MOTION_ACTIVE_LEVEL on motion above thresholdMg (1..255 mg).
    // blankingSamples (0..63) are ignored after arming, to screen out the
    // jolt of setting the device down.  lowPowerHz: 128, 21, 11 or 3 (lower
    // = less current, slower to notice).
    //
    // Call it right before power.deepSleep(seconds, true): re-arming resets
    // INT1 to its idle level, so a stale event can't wake the board at once.
    // After this, begin() is needed to get data again.
    bool enableWakeOnMotion(uint8_t thresholdMg = 120, uint8_t blankingSamples = 32,
                            uint8_t lowPowerHz = 21);

    // True if a WoM event has happened since the last call (reads and clears
    // STATUS1).  Usable awake, too: poll it, or watch GPIO21.
    bool motionDetected();

    // Both sensors off: the chip's lowest-power state short of losing power,
    // which this board can't do.  begin() brings it back.
    void sleep();

    // Raw register access on the shared bus.
    bool readReg(uint8_t reg, uint8_t *buf, size_t len);
    bool writeReg(uint8_t reg, uint8_t value);

private:
    AMOLED206_Board &_board;
    bool _ok = false;
    bool _wom = false;

    bool _reset();
    bool _command(uint8_t cmd);
};
