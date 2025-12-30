#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_PN532.h>

// ===================== PINS (YOUR SETUP) =====================
#define TFT_CS_PIN   15

#define TOUCH_CS     5
#define TOUCH_IRQ    255    // IRQ not working (stays 1), so disable it

#define PN532_SDA    32
#define PN532_SCL    33

// ===================== OBJECTS =====================
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Adafruit_PN532 nfc(-1, -1);

// Try 0/1/2/3 if rotation is wrong
int TOUCH_ROTATION = 1;

// ===================== TOUCH FILTER SETTINGS =====================
// If you still get false touches, raise Z_MIN to 1200 or 1500
static const int Z_MIN = 800;     // minimum pressure to accept
static const int Z_MAX = 3500;    // ignore garbage like 4095
static const int X_MIN = 100, X_MAX = 3900;
static const int Y_MIN = 100, Y_MAX = 3900;

// Take 5 quick samples, require at least 3 good ones
static const int TOUCH_SAMPLES = 5;
static const int TOUCH_NEED_GOOD = 3;

// ===================== UI HELPERS =====================
void drawUI() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("TFT + Touch + RFID");

  tft.drawFastHLine(0, 40, tft.width(), TFT_DARKGREY);

  tft.setCursor(10, 55);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("RFID:");

  tft.setCursor(10, 125);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.println("Touch:");
}

void showRFIDStatus(const char* msg, uint16_t color) {
  tft.fillRect(10, 80, tft.width() - 20, 30, TFT_BLACK);
  tft.setCursor(10, 80);
  tft.setTextColor(color, TFT_BLACK);
  tft.println(msg);
}

void showUID(uint8_t* uid, uint8_t uidLength) {
  tft.fillRect(10, 80, tft.width() - 20, 30, TFT_BLACK);
  tft.setCursor(10, 80);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);

  for (uint8_t i = 0; i < uidLength; i++) {
    if (uid[i] < 0x10) tft.print("0");
    tft.print(uid[i], HEX);
    tft.print(" ");
  }
}

void showTouch(int x, int y, int z) {
  tft.fillRect(10, 150, tft.width() - 20, 30, TFT_BLACK);
  tft.setCursor(10, 150);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.printf("x:%d y:%d z:%d", x, y, z);
}

// ===================== STABLE TOUCH READ =====================
// Returns true only if it got consistent valid samples (not noise)
bool readTouchStable(int &x, int &y, int &z) {
  int good = 0;
  long sx = 0, sy = 0, sz = 0;

  for (int i = 0; i < TOUCH_SAMPLES; i++) {
    // Release TFT from SPI while reading touch (prevents bus conflicts)
    tft.endWrite();
    digitalWrite(TFT_CS_PIN, HIGH);

    TS_Point p = ts.getPoint();

    // Reject common garbage readings
    if (p.z < Z_MIN || p.z > Z_MAX) { delay(2); continue; }
    if (p.x < X_MIN || p.x > X_MAX) { delay(2); continue; }
    if (p.y < Y_MIN || p.y > Y_MAX) { delay(2); continue; }

    sx += p.x;
    sy += p.y;
    sz += p.z;
    good++;

    delay(2);
  }

  if (good >= TOUCH_NEED_GOOD) {
    x = sx / good;
    y = sy / good;
    z = sz / good;
    return true;
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // ---------- TFT ----------
  tft.init();
  tft.setRotation(1);

  // Ensure CS pins are outputs and default HIGH
  pinMode(TFT_CS_PIN, OUTPUT);
  digitalWrite(TFT_CS_PIN, HIGH);

  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);

  drawUI();

  // ---------- Touch ----------
  SPI.begin();
  ts.begin();
  ts.setRotation(TOUCH_ROTATION);

  // ---------- RFID (PN532 I2C) ----------
  Wire.begin(PN532_SDA, PN532_SCL);
  nfc.begin();

  uint32_t version = nfc.getFirmwareVersion();
  if (!version) {
    Serial.println("PN532 NOT found");
    showRFIDStatus("PN532 NOT FOUND", TFT_RED);
  } else {
    nfc.SAMConfig();
    Serial.print("PN532 OK, FW=0x");
    Serial.println(version, HEX);
    showRFIDStatus("READY - scan tag", TFT_GREEN);
  }
}

void loop() {
  // ---------- Touch ----------
  int tx=0, ty=0, tz=0;
  if (readTouchStable(tx, ty, tz)) {
    Serial.printf("TOUCH x=%d y=%d z=%d\n", tx, ty, tz);
    showTouch(tx, ty, tz);
    delay(150); // debounce so it doesn't spam
  }

  // ---------- RFID ----------
  uint8_t uid[7];
  uint8_t uidLength;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 20)) {
    Serial.print("Tag UID: ");
    for (uint8_t i = 0; i < uidLength; i++) {
      if (uid[i] < 0x10) Serial.print("0");
      Serial.print(uid[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    showUID(uid, uidLength);
    delay(400); // prevent tag spam
    showRFIDStatus("READY - scan tag", TFT_GREEN);
  }

  delay(20);
}
