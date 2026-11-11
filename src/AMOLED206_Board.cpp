// AMOLED206_Board - shared I2C bus, pin map and peripheral rails for the
// Waveshare ESP32-S3-Touch-AMOLED-2.06.
//
// Copyright (c) 2026 chipguyhere
// MIT License

#include "AMOLED206_Board.h"
#include "AMOLED206_OrientationWatch.h"
#include <driver/gpio.h>

// AXP2101 registers used here (the power library has the rest).
static constexpr uint8_t AXP_REG_CHIP_ID   = 0x03;   // reads 0x4A
static constexpr uint8_t AXP_CHIP_ID       = 0x4A;
static constexpr uint8_t AXP_REG_LDO_EN0   = 0x90;   // bit0 ALDO1 .. bit3 ALDO4
static constexpr uint8_t AXP_REG_ALDO1_V   = 0x92;   // ALDO1..4 at 0x92..0x95
                                                     // 0.5 V + 0.1 V * n, bits 4:0

AMOLED206_Board::AMOLED206_Board()
    : Wire(::Wire, SDA_PIN, SCL_PIN), _begun(false) {}

bool AMOLED206_Board::begin() {
    if (_begun) return true;

    // After a deep-sleep wake: if the orientation watch armed the ULP, stop it
    // and take GPIO14/15 back from the RTC domain BEFORE touching the bus;
    // and release the reset-line holds the power library set for sleep.
    AMOLED206_OrientationWatch::reclaimBus();
    gpio_hold_dis((gpio_num_t)LCD_RESET_PIN);
    gpio_hold_dis((gpio_num_t)TOUCH_RESET_PIN);

    Wire.begin();   // ThreadsafeWire on this board's SDA/SCL
    Wire.setClock(400000);

    // GPIO0 has its own 10k pull-up; INPUT_PULLUP is harmless.  GPIO10 has a
    // 10k pull-up to VCC3V3 and is driven low by the BSS138 when idle.
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
    pinMode(PWR_BUTTON_PIN, INPUT);

    // The amplifier has a 10k pull-down on CTRL; drive it low explicitly so
    // the speaker stays quiet until a sketch asks for it.  The motor's base
    // likewise has a 47k pull-down, but make it an output now.
    pinMode(AMP_ENABLE_PIN, OUTPUT);
    digitalWrite(AMP_ENABLE_PIN, LOW);
    pinMode(MOTOR_PIN, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);

    _begun = true;

    // ALDO1 must be on for the bus to work at all (an unpowered ES7210 clamps
    // it).  Make sure, and say loudly if the bus is already dead.
    uint8_t id = 0;
    if (!Wire.readRegister(AXP2101_ADDR, 0x03, id) || id != 0x4A) {
        Serial.println("AMOLED206_Board: the AXP2101 PMIC does not answer on I2C. If a "
                       "sketch switched the PMIC's ALDO1 rail off, the unpowered ES7210 "
                       "is holding the bus down: remove ALL power (USB and battery) for "
                       "a few seconds to reset the PMIC.");
    } else {
        _setAldo(0, true, 3300);
    }
    return true;
}

// ── Probes ──────────────────────────────────────────────────────────────────

bool AMOLED206_Board::hasPmic() {
    begin();
    uint8_t id = 0;
    return Wire.readRegister(AXP2101_ADDR, AXP_REG_CHIP_ID, id) && id == AXP_CHIP_ID;
}

bool AMOLED206_Board::hasTouch() {
    begin();
    return Wire.probe(FT3168_ADDR);
}

bool AMOLED206_Board::hasRtc() {
    begin();
    return Wire.probe(PCF85063_ADDR);
}

bool AMOLED206_Board::hasImu() {
    begin();
    uint8_t who = 0;
    return Wire.readRegister(QMI8658_ADDR, 0x00, who) && who == 0x05;
}

bool AMOLED206_Board::hasCodec() {
    begin();
    return Wire.probe(ES8311_ADDR);
}

bool AMOLED206_Board::hasMicAdc() {
    begin();
    return Wire.probe(ES7210_ADDR);
}

// ── Buttons ─────────────────────────────────────────────────────────────────

bool AMOLED206_Board::bootButtonPressed() {
    return digitalRead(BOOT_BUTTON_PIN) == LOW;
}

bool AMOLED206_Board::pwrButtonPressed() {
    return digitalRead(PWR_BUTTON_PIN) == AMOLED206_PWR_ACTIVE_LEVEL;
}

// ── PMIC register access ────────────────────────────────────────────────────

bool AMOLED206_Board::pmicRead(uint8_t reg, uint8_t &value) {
    begin();
    return Wire.readRegister(AXP2101_ADDR, reg, value);
}

bool AMOLED206_Board::pmicWrite(uint8_t reg, uint8_t value) {
    begin();
    return Wire.writeRegister(AXP2101_ADDR, reg, value);
}

bool AMOLED206_Board::pmicUpdate(uint8_t reg, uint8_t clearMask, uint8_t setMask) {
    begin();
    // Read-modify-write must be atomic against the power library doing the
    // same to the same register from another task.
    ThreadsafeWire::Guard g(Wire);
    if (!g) return false;
    uint8_t v;
    if (!Wire.readRegister(AXP2101_ADDR, reg, v)) return false;
    uint8_t nv = (uint8_t)((v & ~clearMask) | setMask);
    if (nv == v) return true;
    return Wire.writeRegister(AXP2101_ADDR, reg, nv);
}

// index 0..3 = ALDO1..ALDO4.  mv == 0 leaves the voltage alone.
bool AMOLED206_Board::_setAldo(uint8_t index, bool on, uint16_t mv) {
    if (on && mv) {
        if (mv < 500) mv = 500;
        if (mv > 3500) mv = 3500;
        uint8_t code = (uint8_t)((mv - 500) / 100);
        if (!pmicUpdate(AXP_REG_ALDO1_V + index, 0x1F, code)) return false;
    }
    uint8_t bit = (uint8_t)(1u << index);
    return pmicUpdate(AXP_REG_LDO_EN0, on ? 0 : bit, on ? bit : 0);
}

// ── Rails ───────────────────────────────────────────────────────────────────

bool AMOLED206_Board::audioPower(bool on) {
    // Never cut ALDO1: it powers the ES7210 completely, and an unpowered
    // ES7210 clamps the shared I2C bus (see the header).
    if (!on) return false;
    bool ok = _setAldo(0, true, 3300);      // ALDO1 = A3V3
    if (ok) delay(5);                       // let the rail settle
    return ok;
}

void AMOLED206_Board::ampEnable(bool on) {
    begin();
    digitalWrite(AMP_ENABLE_PIN, on ? HIGH : LOW);
}

bool AMOLED206_Board::panelPower(bool on) {
    // Never turn ALDO2 off: measured on the bench (2026-10-06), the board
    // draws ~13 mA MORE in deep sleep with the panel's power enable low.
    if (!on) return false;
    return _setAldo(1, true, 3300);         // ALDO2 -> DSI_PWR_EN via 10k
}

bool AMOLED206_Board::motorPower(bool on) {
    if (!on) digitalWrite(MOTOR_PIN, LOW);
    return _setAldo(2, on, 0);              // ALDO3, voltage as OTP left it
}

void AMOLED206_Board::vibrate(bool on) {
    begin();
    if (on) {
        uint8_t en = 0;
        if (pmicRead(AXP_REG_LDO_EN0, en) && !(en & 0x04)) motorPower(true);
    }
    digitalWrite(MOTOR_PIN, on ? HIGH : LOW);
}
