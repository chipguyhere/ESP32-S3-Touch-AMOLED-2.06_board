// AMOLED206_IMU - QMI8658 on the Waveshare ESP32-S3-Touch-AMOLED-2.06.
//
// Copyright (c) 2026 chipguyhere
// MIT License

#include "AMOLED206_IMU.h"

// QMI8658 registers.
#define QMI_WHO_AM_I    0x00   // 0x05
#define QMI_CTRL1       0x02   // b6 address auto-increment, b3 INT1 enable, b4 INT2 enable
#define QMI_CTRL2       0x03   // accel: b6:4 range, b3:0 ODR
#define QMI_CTRL3       0x04   // gyro:  b6:4 range, b3:0 ODR
#define QMI_CTRL7       0x08   // b0 accel enable, b1 gyro enable
#define QMI_CTRL9       0x0A   // host command
#define QMI_CAL1_L      0x0B   // WoM threshold, mg
#define QMI_CAL1_H      0x0C   // WoM b7:6 pin + initial level, b5:0 blanking samples
#define QMI_STATUSINT   0x2D   // b7 CmdDone
#define QMI_STATUS0     0x2E   // b0 accel data ready, b1 gyro data ready
#define QMI_STATUS1     0x2F   // b2 WoM (clears on read)
#define QMI_TEMP_L      0x33
#define QMI_AX_L        0x35
#define QMI_GX_L        0x3B
#define QMI_RST_RESULT  0x4D   // 0x80 when a reset has completed
#define QMI_RESET       0x60   // write 0xB0

#define QMI_CMD_ACK            0x00
#define QMI_CMD_WRITE_WOM      0x08

static constexpr uint8_t ADDR = AMOLED206_Board::QMI8658_ADDR;

bool AMOLED206_IMU::readReg(uint8_t reg, uint8_t *buf, size_t len) {
    return _board.Wire.readRegister(ADDR, reg, buf, len);
}

bool AMOLED206_IMU::writeReg(uint8_t reg, uint8_t value) {
    return _board.Wire.writeRegister(ADDR, reg, value);
}

bool AMOLED206_IMU::_reset() {
    if (!writeReg(QMI_RESET, 0xB0)) return false;
    uint32_t t0 = millis();
    do {
        delay(10);
        uint8_t r;
        if (readReg(QMI_RST_RESULT, &r, 1) && r == 0x80) {
            // Address auto-increment on, everything else left at its reset
            // value -- exactly what SensorLib does, so the data byte order
            // below (little-endian) is the one the vendor examples rely on.
            uint8_t c1;
            if (!readReg(QMI_CTRL1, &c1, 1)) return false;
            return writeReg(QMI_CTRL1, (uint8_t)(c1 | 0x40));
        }
    } while (millis() - t0 < 500);
    return false;
}

// CTRL9 handshake: write the command, wait for CmdDone, acknowledge, wait for
// CmdDone to clear.
bool AMOLED206_IMU::_command(uint8_t cmd) {
    if (!writeReg(QMI_CTRL9, cmd)) return false;
    uint32_t t0 = millis();
    uint8_t s = 0;
    do {
        delay(1);
        if (millis() - t0 > 1000) return false;
    } while (!(readReg(QMI_STATUSINT, &s, 1) && (s & 0x80)));
    if (!writeReg(QMI_CTRL9, QMI_CMD_ACK)) return false;
    t0 = millis();
    do {
        delay(1);
        if (millis() - t0 > 1000) return false;
    } while (!(readReg(QMI_STATUSINT, &s, 1) && !(s & 0x80)));
    return true;
}

bool AMOLED206_IMU::begin() {
    _board.begin();
    _ok = false;
    _wom = false;
    // INT1 is push-pull from the IMU; no pull needed.
    pinMode(AMOLED206_Board::IMU_INT1_PIN, INPUT);

    uint8_t who = 0;
    if (!readReg(QMI_WHO_AM_I, &who, 1) || who != 0x05) return false;
    if (!_reset()) return false;

    writeReg(QMI_CTRL7, 0x00);                 // sensors off while configuring
    writeReg(QMI_CTRL2, (1 << 4) | 6);         // accel +/-4 g, ~125 Hz
    writeReg(QMI_CTRL3, (5 << 4) | 6);         // gyro +/-512 dps, ~112 Hz
    writeReg(QMI_CTRL7, 0x03);                 // accel + gyro on

    // Wait for the first samples (STATUS0 b0 accel, b1 gyro ready).  Measured
    // on the bench: they take more than 30 ms, and reconfiguring the sensors
    // before the first sample lands leaves the accelerometer reading zeros
    // until the next reset -- so begin() doesn't return until data flows.
    uint32_t t0 = millis();
    uint8_t st = 0;
    while (millis() - t0 < 300) {
        delay(5);
        if (readReg(QMI_STATUS0, &st, 1) && (st & 0x03) == 0x03) break;
    }
    _ok = true;
    return true;
}

static bool readTriple(AMOLED206_IMU &imu, uint8_t reg, float scale,
                       float &x, float &y, float &z) {
    uint8_t b[6];
    if (!imu.readReg(reg, b, 6)) return false;
    x = (int16_t)(b[0] | (b[1] << 8)) * scale;
    y = (int16_t)(b[2] | (b[3] << 8)) * scale;
    z = (int16_t)(b[4] | (b[5] << 8)) * scale;
    return true;
}

bool AMOLED206_IMU::readAccel(float &x, float &y, float &z) {
    if (!_ok || _wom) return false;
    return readTriple(*this, QMI_AX_L, 4.0f / 32768.0f, x, y, z);
}

bool AMOLED206_IMU::readGyro(float &x, float &y, float &z) {
    if (!_ok || _wom) return false;
    return readTriple(*this, QMI_GX_L, 512.0f / 32768.0f, x, y, z);
}

float AMOLED206_IMU::readTemperature() {
    uint8_t b[2];
    if (!readReg(QMI_TEMP_L, b, 2)) return NAN;
    return (int8_t)b[1] + b[0] / 256.0f;
}

bool AMOLED206_IMU::enableWakeOnMotion(uint8_t thresholdMg, uint8_t blankingSamples,
                                       uint8_t lowPowerHz) {
    _board.begin();
    pinMode(AMOLED206_Board::IMU_INT1_PIN, INPUT);
    if (!_reset()) return false;
    _ok = true;

    uint8_t odr = lowPowerHz >= 128 ? 12 : lowPowerHz >= 21 ? 13 : lowPowerHz >= 11 ? 14 : 15;
    if (thresholdMg == 0) thresholdMg = 1;

    writeReg(QMI_CTRL7, 0x00);                 // sensors off
    writeReg(QMI_CTRL2, (2 << 4) | odr);       // accel +/-8 g, low-power ODR, gyro stays off
    writeReg(QMI_CAL1_L, thresholdMg);

    // CAL1_H b7:6 -- 00 = INT1 idle low, 10 = INT1 idle high.  Idle at the
    // INACTIVE level so the event drives it to the active one.
    bool idleHigh = (AMOLED206_MOTION_ACTIVE_LEVEL == LOW);
    uint8_t sel = idleHigh ? 0x02 : 0x00;
    writeReg(QMI_CAL1_H, (uint8_t)((sel << 6) | (blankingSamples & 0x3F)));
    if (!_command(QMI_CMD_WRITE_WOM)) return false;

    writeReg(QMI_CTRL7, 0x01);                 // accelerometer on
    uint8_t c1;
    if (!readReg(QMI_CTRL1, &c1, 1)) return false;
    writeReg(QMI_CTRL1, (uint8_t)(c1 | 0x08)); // INT1 enable

    uint8_t s;
    readReg(QMI_STATUS1, &s, 1);               // drop anything latched while arming
    _wom = true;
    return true;
}

bool AMOLED206_IMU::motionDetected() {
    uint8_t s;
    return readReg(QMI_STATUS1, &s, 1) && (s & 0x04);
}

void AMOLED206_IMU::sleep() {
    writeReg(QMI_CTRL7, 0x00);
}
