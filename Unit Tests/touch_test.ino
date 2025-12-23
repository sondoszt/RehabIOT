/*
  Unit Test: TFT + XPT2046 Touchscreen
  Board: ESP32
  Purpose: Validate display and touch panel wiring and raw touch readings
*/

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// =====================
// TFT_eSPI uses pins from User_Setup.h
// (MISO12 MOSI13 SCK14 CS15 DC2 RST-1 BL21)
// =====================
TFT_eSPI tft = TFT_eSPI();

// =====================
// Touch wiring (from your table)
// =====================
#define TOUCH_IRQ 36   // T_IRQ
#define TOUCH_MOSI 32  // T_DIN
#define TOUCH_MISO 39  // T_OUT
#define TOUCH_SCK  25  // T_CLK
#define TOUCH_CS   33  // T_CS

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

#define SCREEN_W 320
#define SCREEN_H 240

void setup() {
  Serial.begin(115200);
  delay(200);

  // ---- Init TFT ----
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawCentreString("TFT + TOUCH TEST", SCREEN_W/2, 10, 2);
  tft.drawCentreString("Touch screen with PEN", SCREEN_W/2, 40, 2);

  // ---- Init TOUCH on its own SPI pins ----
  touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  Serial.println("Ready. Touch the screen...");
}

void drawTouch(int x, int y, int z) {
  tft.fillRect(0, 80, SCREEN_W, 120, TFT_WHITE);
  tft.setCursor(10, 90);
  tft.setTextSize(2);
  tft.printf("X=%d\nY=%d\nZ=%d\n", x, y, z);

  // show a dot where you touched (after mapping)
  tft.fillCircle(x, y, 4, TFT_RED);
}

void loop() {
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();

    // RAW values (print first!)
    Serial.printf("RAW: x=%d y=%d z=%d\n", p.x, p.y, p.z);

    // These map ranges are *examples* — we will calibrate after we confirm it changes
    int x = map(p.x, 200, 3700, 0, SCREEN_W);
    int y = map(p.y, 240, 3800, 0, SCREEN_H);

    // clamp
    if (x < 0) x = 0; if (x >= SCREEN_W) x = SCREEN_W - 1;
    if (y < 0) y = 0; if (y >= SCREEN_H) y = SCREEN_H - 1;

    drawTouch(x, y, p.z);
    delay(120);
  }
}
