#pragma once

// Board abstraction for the Waveshare ESP32-S3-Touch-AMOLED-2.06
// (2.06" 410x502 CO5300 AMOLED "watch" board, schematic
// ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0).
//
// Everything on this board that is not the display's QSPI bus or the audio
// I2S bus hangs off ONE I2C bus (GPIO15 SDA / GPIO14 SCL): the FT3168 touch
// controller, the AXP2101 PMIC, the PCF85063 RTC, the QMI8658 IMU, the ES8311
// codec and the ES7210 microphone ADC.  This class owns that bus, wrapped in a
// mutex-protected ThreadsafeWire so tasks and drivers can share it safely, and
// it is the one place the board's pin map and I2C address map are written
// down so the display, power and sketch code can use names instead of numbers.
//
// It also owns the three PMIC rails that exist only to feed board peripherals
// -- ALDO1 (audio analog), ALDO2 (the AMOLED's power-enable pull-up) and ALDO3
// (the vibration motor) -- because the code that uses those peripherals needs
// to switch them without dragging in the whole power library.  The PMIC as a
// power manager (battery, charger, power key, shutdown) is
// chipguy_ESP32-S3-Touch-AMOLED-2.06_power.
//
// The ThreadsafeWire wrapper lives in chipguy_ThreadsafeWire.h -- a verbatim copy
// of the one in chipguy_Board, vendored so this library depends on nothing but
// Wire.  Its unit of locking is the TRANSACTION, so an ordinary
// beginTransmission/write/endTransmission sequence is already atomic against
// every other task, with no Guard of its own.
//
// Copyright (c) 2026 chipguyhere
// MIT License

#include <Arduino.h>
#include <Wire.h>

// The shared-bus wrapper, vendored from chipguy_Board so this library depends on
// nothing but Wire. Kept in its own header so device drivers can be written
// against ThreadsafeWire without including this board class. Copy it forward
// when chipguy_Board's copy changes; do not fork it.
#include "chipguy_ThreadsafeWire.h"

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "AMOLED206_Board: this board is an ESP32-S3. Select ESP32S3 Dev Module."
#endif

// The PWR button reaches GPIO10 through a BSS138 inverter (see
// pwrButtonPressed()).  The pin reads HIGH while the button is held --
// inferred from the schematic and measured on one unit (2026-10-05).  The
// macro stays in case another board revision differs.
#ifndef AMOLED206_PWR_ACTIVE_LEVEL
#define AMOLED206_PWR_ACTIVE_LEVEL HIGH
#endif

class AMOLED206_Board {
public:
    // ── Shared I2C bus ──────────────────────────────────────────────────────
    // Pull-ups: 2.2k to VCC3V3 (R23/R49, by the codec).  Also on test pads
    // TP5 (SCL) / TP7 (SDA).
    static constexpr int SDA_PIN = 15;
    static constexpr int SCL_PIN = 14;

    // 7-bit I2C addresses of everything on the bus.
    static constexpr uint8_t FT3168_ADDR   = 0x38;  // touch controller
    static constexpr uint8_t AXP2101_ADDR  = 0x34;  // PMIC
    static constexpr uint8_t PCF85063_ADDR = 0x51;  // RTC
    static constexpr uint8_t QMI8658_ADDR  = 0x6B;  // IMU (SA0 strapped low)
    static constexpr uint8_t ES8311_ADDR   = 0x18;  // audio codec (CE pulled low)
    static constexpr uint8_t ES7210_ADDR   = 0x40;  // mic ADC (AD0/AD1 low)

    // ── Display (CO5300 AMOLED, QSPI) ───────────────────────────────────────
    // Driven by chipguy_ESP32-S3-Touch-AMOLED-2.06_display; listed for the map.
    static constexpr int LCD_SIO0_PIN  = 4;
    static constexpr int LCD_SIO1_PIN  = 5;
    static constexpr int LCD_SIO2_PIN  = 6;
    static constexpr int LCD_SIO3_PIN  = 7;
    static constexpr int LCD_SCLK_PIN  = 11;
    static constexpr int LCD_CS_PIN    = 12;
    static constexpr int LCD_RESET_PIN = 8;
    static constexpr int LCD_TE_PIN    = 13;   // tearing-effect output from the panel

    // ── Touch (FT3168, on the shared bus) ───────────────────────────────────
    static constexpr int TOUCH_INT_PIN   = 38;  // active LOW
    static constexpr int TOUCH_RESET_PIN = 9;   // active LOW

    // ── Audio (ES8311 codec + ES7210 mic ADC on one I2S port) ───────────────
    static constexpr int I2S_MCLK_PIN = 16;
    static constexpr int I2S_BCLK_PIN = 41;
    static constexpr int I2S_WS_PIN   = 45;   // strapping pin; output after boot
    static constexpr int I2S_DOUT_PIN = 40;   // ESP32 -> ES8311 DAC (DSDIN)
    static constexpr int I2S_DIN_PIN  = 42;   // ES7210 SDOUT1 -> ESP32 (via 51R)
    static constexpr int AMP_ENABLE_PIN = 46; // NS4150B CTRL, HIGH = on; 10k pull-down; strapping pin

    // ── Vibration motor ─────────────────────────────────────────────────────
    static constexpr int MOTOR_PIN = 18;      // HIGH = motor on (low-side NPN), needs ALDO3

    // ── Sensors ─────────────────────────────────────────────────────────────
    static constexpr int IMU_INT1_PIN = 21;   // QMI8658 INT1 (an RTC GPIO: can wake deep sleep)
    static constexpr int RTC_INT_PIN  = 39;   // PCF85063 INT, open drain, NO pull-up on the board

    // ── TF / microSD slot (SPI mode, 10k pull-ups on all four lines) ────────
    static constexpr int SD_CS_PIN   = 17;
    static constexpr int SD_MOSI_PIN = 1;
    static constexpr int SD_CLK_PIN  = 2;
    static constexpr int SD_MISO_PIN = 3;

    // ── Buttons ─────────────────────────────────────────────────────────────
    static constexpr int BOOT_BUTTON_PIN = 0;   // active LOW, 10k pull-up
    static constexpr int PWR_BUTTON_PIN  = 10;  // PWR, via BSS138 inverter; see pwrButtonPressed()

    // ── Native USB and the console UART ─────────────────────────────────────
    static constexpr int USB_DN_PIN = 19;
    static constexpr int USB_DP_PIN = 20;
    static constexpr int U0TXD_PIN  = 43;   // test pad TP13 only
    static constexpr int U0RXD_PIN  = 44;   // test pad TP6 only

    AMOLED206_Board();

    // Bring up the shared I2C bus and configure the button inputs.
    // Idempotent: the display, touch and power libraries all call it, only the
    // first does the work.
    bool begin();

    // The shared, mutex-protected I2C bus.  Every transaction locks itself, so
    // a Guard is needed only to make SEVERAL transactions atomic as a group.
    ThreadsafeWire Wire;

    // ── What answers on the bus?  (call after begin()) ──────────────────────
    bool hasPmic();         // AXP2101 answers AND reports chip ID 0x4A
    bool hasTouch();        // FT3168 answers (after the display library has reset it)
    bool hasRtc();          // PCF85063
    bool hasImu();          // QMI8658 (WHO_AM_I = 0x05)
    bool hasCodec();        // ES8311
    bool hasMicAdc();       // ES7210

    // ── Buttons ─────────────────────────────────────────────────────────────
    // True while the BOOT button is held.  GPIO0 with a hardware pull-up, so
    // holding it through a power-up enters the bootloader.
    bool bootButtonPressed();

    // True while the PWR button is held.  PWR goes to the AXP2101's PWRON pin
    // (which is what powers the board on and, held long enough, off) and,
    // through a BSS138 inverter, to GPIO10 (HIGH while held, measured).  PWR
    // is the LOWER of the two side buttons with the text upright in portrait;
    // BOOT is the upper one.
    bool pwrButtonPressed();

    // ── Sound: the two calls every chipguy board with sound provides ────────
    // audioPower(): the AXP2101's ALDO1, the "A3V3" rail that feeds the
    // ES8311's analog side and ALL of the ES7210 mic ADC, digital supply
    // included.  on=true sets it to 3.3 V and enables it and returns true.
    //
    // on=false does NOT cut the rail, and returns false.  Measured on the
    // bench (2026-10-05): with ALDO1 off, the unpowered ES7210 clamps the
    // shared I2C bus and EVERY device on it -- PMIC, touch, RTC, IMU -- stops
    // answering, so the rail can't even be switched back on; only removing
    // all power (USB and battery) recovers.  Quiet the codecs with their own
    // power-down registers instead.
    bool audioPower(bool on);

    // ampEnable(): the NS4150B speaker amplifier's CTRL pin (GPIO46).
    void ampEnable(bool on);

    // ── The other peripheral rails ──────────────────────────────────────────
    // panelPower(): ALDO2, which pulls up the AMOLED module's DSI_PWR_EN pin.
    // The display library turns it on in begin() (3.3 V).  on=false does NOT
    // cut it and returns false: measured on the bench (2026-10-06), with
    // DSI_PWR_EN low the board draws ~13 mA MORE in deep sleep (15.3 mA
    // against 2.2 mA).  Use display.sleep() to quiet the panel instead.
    bool panelPower(bool on);

    // motorPower(): ALDO3, the vibration motor's supply.  Voltage is left at
    // whatever the PMIC's OTP chose (the schematic's table says ~3 V).
    bool motorPower(bool on);

    // Run or stop the vibration motor.  on=true turns ALDO3 on first if
    // needed.  The motor has no flyback diode on this board; keep pulses short.
    void vibrate(bool on);

    // Raw AXP2101 register access, serialised on the shared bus.  The power
    // library has a full driver; these exist for the rail helpers above.
    bool pmicRead(uint8_t reg, uint8_t &value);
    bool pmicWrite(uint8_t reg, uint8_t value);
    bool pmicUpdate(uint8_t reg, uint8_t clearMask, uint8_t setMask);

private:
    bool _begun;
    bool _setAldo(uint8_t index, bool on, uint16_t mv);
};
