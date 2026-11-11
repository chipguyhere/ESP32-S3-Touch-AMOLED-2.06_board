// BoardScan - prints the Waveshare ESP32-S3-Touch-AMOLED-2.06 pin map, says
// which chips answer on the shared I2C bus, and shows the two buttons live.
//
// Board settings: ESP32S3 Dev Module, PSRAM "OPI PSRAM", USB CDC On Boot
// "Enabled".  Open the Serial Monitor at 115200.
//
// Press BOOT: a short buzz from the vibration motor.

#include <AMOLED206_Board.h>

AMOLED206_Board board;

static void yesNo(const char *what, bool ok) {
    Serial.printf("  %-34s %s\n", what, ok ? "yes" : "NO");
}

void setup() {
    Serial.begin(115200);
    // The USB-C port is the S3's own USB: if nothing on the computer is reading
    // it, each print would wait for a timeout and the sketch would stutter.
    Serial.setTxTimeoutMs(0);
    delay(1500);   // give the USB console a moment to attach
    board.begin();

    Serial.println();
    Serial.println("Waveshare ESP32-S3-Touch-AMOLED-2.06");
    Serial.println("Shared I2C bus: SDA GPIO15, SCL GPIO14");
    yesNo("AXP2101 PMIC        (0x34)", board.hasPmic());
    yesNo("FT3168 touch        (0x38)", board.hasTouch());
    yesNo("PCF85063 RTC        (0x51)", board.hasRtc());
    yesNo("QMI8658 IMU         (0x6B)", board.hasImu());

    // The codec and mic ADC run their I2C from VCC3V3, but power them up
    // anyway so a scan reflects a board that can actually make sound.
    board.audioPower(true);
    yesNo("ES8311 codec        (0x18)", board.hasCodec());
    yesNo("ES7210 mic ADC      (0x40)", board.hasMicAdc());

    Serial.println("Full 7-bit address scan:");
    Serial.print("  ");
    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (board.Wire.probe(a)) Serial.printf("0x%02X ", a);
    }
    Serial.println();

    Serial.println("Pins:");
    Serial.printf("  display QSPI  SIO0..3 %d %d %d %d  SCLK %d  CS %d  RESET %d  TE %d\n",
                  AMOLED206_Board::LCD_SIO0_PIN, AMOLED206_Board::LCD_SIO1_PIN,
                  AMOLED206_Board::LCD_SIO2_PIN, AMOLED206_Board::LCD_SIO3_PIN,
                  AMOLED206_Board::LCD_SCLK_PIN, AMOLED206_Board::LCD_CS_PIN,
                  AMOLED206_Board::LCD_RESET_PIN, AMOLED206_Board::LCD_TE_PIN);
    Serial.printf("  touch         INT %d  RESET %d\n",
                  AMOLED206_Board::TOUCH_INT_PIN, AMOLED206_Board::TOUCH_RESET_PIN);
    Serial.printf("  I2S           MCLK %d  BCLK %d  WS %d  DOUT %d  DIN %d  amp %d\n",
                  AMOLED206_Board::I2S_MCLK_PIN, AMOLED206_Board::I2S_BCLK_PIN,
                  AMOLED206_Board::I2S_WS_PIN, AMOLED206_Board::I2S_DOUT_PIN,
                  AMOLED206_Board::I2S_DIN_PIN, AMOLED206_Board::AMP_ENABLE_PIN);
    Serial.printf("  SD (SPI)      CS %d  MOSI %d  CLK %d  MISO %d\n",
                  AMOLED206_Board::SD_CS_PIN, AMOLED206_Board::SD_MOSI_PIN,
                  AMOLED206_Board::SD_CLK_PIN, AMOLED206_Board::SD_MISO_PIN);
    Serial.printf("  IMU INT1 %d  RTC INT %d  motor %d  BOOT %d  PWR %d\n",
                  AMOLED206_Board::IMU_INT1_PIN, AMOLED206_Board::RTC_INT_PIN,
                  AMOLED206_Board::MOTOR_PIN, AMOLED206_Board::BOOT_BUTTON_PIN,
                  AMOLED206_Board::PWR_BUTTON_PIN);
    Serial.println();
    Serial.println("Buttons (live):");
}

void loop() {
    static bool lastBoot = false, lastPwr = false;
    bool boot = board.bootButtonPressed();
    bool pwr  = board.pwrButtonPressed();
    if (boot != lastBoot || pwr != lastPwr) {
        Serial.printf("  BOOT %s   PWR %s   (GPIO10 reads %s)\n",
                      boot ? "down" : "up  ", pwr ? "down" : "up  ",
                      digitalRead(AMOLED206_Board::PWR_BUTTON_PIN) ? "HIGH" : "LOW");
        if (boot && !lastBoot) {
            board.vibrate(true);
            delay(80);
            board.vibrate(false);
        }
        lastBoot = boot;
        lastPwr = pwr;
    }
    delay(20);
}
