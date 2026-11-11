// AMOLED206_OrientationWatch - see the header.
//
// Copyright (c) 2026 chipguyhere
// MIT License

#include "AMOLED206_OrientationWatch.h"
#include "amoled206_orient_ulp.h"
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include "ulp_common_defs.h"

// Set while the ULP owns the bus; survives deep sleep so the next boot knows.
RTC_DATA_ATTR static bool s_armed = false;
static int s_words = 0;

int AMOLED206_OrientationWatch::programWords() { return s_words; }

static inline int16_t ulpWord(int w) { return (int16_t)(RTC_SLOW_MEM[w] & 0xFFFF); }

// milli-g -> offset-binary high byte at +/-4 g (32 counts per g)
static int mgToOb(int mg) {
    int hb = (mg >= 0) ? (mg * 32 + 500) / 1000 : -((-mg * 32 + 500) / 1000);
    return constrain(hb + 128, 0, 255);
}

void AMOLED206_OrientationWatch::setWindow(int xLo, int xHi, int yLo, int yHi, int zLo, int zHi) {
    const int lo[3] = { xLo, yLo, zLo }, hi[3] = { xHi, yHi, zHi };
    for (int i = 0; i < 3; i++) {
        if (lo[i] > hi[i]) { _lo[i] = 0; _hi[i] = 255; }       // don't care
        else { _lo[i] = mgToOb(lo[i]); _hi[i] = mgToOb(hi[i]); }
    }
    _taught = true;
}

bool AMOLED206_OrientationWatch::teach(int toleranceMg, int samples) {
    if (!_imu.present() && !_imu.begin()) return false;
    long sx = 0, sy = 0, sz = 0;
    int n = 0;
    for (int i = 0; i < samples; i++) {
        float x, y, z;
        if (_imu.readAccel(x, y, z)) { sx += lroundf(x * 1000); sy += lroundf(y * 1000); sz += lroundf(z * 1000); n++; }
        delay(50);
    }
    if (!n) return false;
    int mx = sx / n, my = sy / n, mz = sz / n;
    setWindow(mx - toleranceMg, mx + toleranceMg, my - toleranceMg, my + toleranceMg,
              mz - toleranceMg, mz + toleranceMg);
    return true;
}

bool AMOLED206_OrientationWatch::save() {
    Preferences p;
    if (!p.begin("amoled206ow", false)) return false;
    p.putBytes("lo", _lo, sizeof _lo);
    p.putBytes("hi", _hi, sizeof _hi);
    p.putUChar("dwell", _dwell);
    p.putUShort("period", _periodMs);
    p.end();
    return true;
}

bool AMOLED206_OrientationWatch::load() {
    Preferences p;
    if (!p.begin("amoled206ow", true)) return false;
    bool ok = p.getBytesLength("lo") == sizeof _lo && p.getBytesLength("hi") == sizeof _hi;
    if (ok) {
        p.getBytes("lo", _lo, sizeof _lo);
        p.getBytes("hi", _hi, sizeof _hi);
        _dwell = p.getUChar("dwell", 2);
        _periodMs = p.getUShort("period", 200);
        _taught = true;
    }
    p.end();
    return ok;
}

bool AMOLED206_OrientationWatch::arm(bool requireExitFirst) {
    _board.begin();

    // IMU: accelerometer only, +/-4 g, low-power 21 Hz.  Wait for the first
    // sample: reconfiguring before it lands leaves the accelerometer at zero.
    if (!_imu.present()) _imu.begin();
    _imu.writeReg(0x08, 0x00);                      // CTRL7: sensors off
    _imu.writeReg(0x03, (1 << 4) | 13);             // CTRL2: +/-4 g, LP 21 Hz
    _imu.writeReg(0x08, 0x01);                      // accelerometer on
    for (int i = 0; i < 60; i++) {
        delay(5);
        uint8_t st = 0;
        if (_imu.readReg(0x2E, &st, 1) && (st & 1)) break;
    }

    amoled206_orient_params_t prm = {};
    for (int i = 0; i < 3; i++) { prm.lo[i] = _lo[i]; prm.hi[i] = _hi[i]; }
    prm.dwell = _dwell;
    prm.delay_cycles = 0;
    prm.wake_enabled = 1;
    prm.require_exit = requireExitFirst ? 1 : 0;
    for (int w = AMOLED206_ORIENT_DATA_BASE; w < 128; w++) RTC_SLOW_MEM[w] = 0;
    if (requireExitFirst) RTC_SLOW_MEM[AMOLED206_ORIENT_EXIT] = 1;
    s_words = amoled206_orient_ulp_build(&prm);
    if (s_words <= 0) return false;
    if (amoled206_orient_ulp_load() != 0) { s_words = -1; return false; }

    // Hand GPIO14/15 to the RTC domain: OUT = 0, output enable toggled by the
    // ULP (open drain); the board's 2.2k pull-ups hold the released level.
    _board.Wire.end();
    const gpio_num_t pins[2] = { (gpio_num_t)AMOLED206_Board::SCL_PIN, (gpio_num_t)AMOLED206_Board::SDA_PIN };
    for (gpio_num_t p : pins) {
        rtc_gpio_init(p);
        rtc_gpio_set_level(p, 0);
        rtc_gpio_set_direction(p, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_dis(p);
        rtc_gpio_pulldown_dis(p);
        gpio_sleep_sel_dis(p);
    }
    s_armed = true;
    if (amoled206_orient_ulp_start((unsigned)_periodMs * 1000u) != 0) { disarm(); return false; }
    esp_sleep_enable_ulp_wakeup();
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    return true;
}

void AMOLED206_OrientationWatch::reclaimBus() {
    if (!s_armed) return;
    amoled206_orient_ulp_stop();
    delay(2);                                       // let a run in flight finish
    rtc_gpio_deinit((gpio_num_t)AMOLED206_Board::SCL_PIN);
    rtc_gpio_deinit((gpio_num_t)AMOLED206_Board::SDA_PIN);
    s_armed = false;
}

void AMOLED206_OrientationWatch::disarm() {
    reclaimBus();
    _board.Wire.begin();
}

bool AMOLED206_OrientationWatch::armed() { return s_armed; }

bool AMOLED206_OrientationWatch::wokeByOrientation() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_ULP;
}

void AMOLED206_OrientationWatch::lastSample(int &x, int &y, int &z) const {
    auto mg = [](int w) { return (int)(int8_t)(ulpWord(w) & 0xFF) * 1000 / 32; };
    x = mg(AMOLED206_ORIENT_AX);
    y = mg(AMOLED206_ORIENT_AY);
    z = mg(AMOLED206_ORIENT_AZ);
}
