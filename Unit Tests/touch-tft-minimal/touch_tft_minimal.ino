#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ===== Your wiring =====
#define TFT_CS      15
#define TFT_DC       2
#define TFT_RST      4

#define TOUCH_CS     5
#define TOUCH_IRQ   27   // if not connected, set to 255
// =======================

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// Rough raw ranges (good enough for this test)
#define RAW_X_MIN 200
#define RAW_X_MAX 3900
#define RAW_Y_MIN 200
#define RAW_Y_MAX 3900

void setup() {
  Serial.begin(115200);
  delay(300);

  // Init TFT
  tft.init();
  tft.setRotation(1);          // try 0/1/2/3 if orientation differs
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Touch -> Dot test", 10, 10, 2);

  // Init SPI + Touch
  SPI.begin(18, 19, 23);       // SCK, MISO, MOSI
  SPI.setFrequency(1000000);
  ts.begin();
  ts.setRotation(1);

  Serial.println("Combined test ready. Touch the screen.");
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    if (p.z > 300) { // pressure threshold
      // Map raw touch to screen pixels (rough)
      int x = map(p.x, RAW_X_MIN, RAW_X_MAX, 0, tft.width());
      int y = map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, tft.height());

      // Clamp
      x = constrain(x, 0, tft.width()-1);
      y = constrain(y, 0, tft.height()-1);

      // Draw a dot
      tft.fillCircle(x, y, 3, TFT_GREEN);

      // Log once in a while
      Serial.print("x_raw="); Serial.print(p.x);
      Serial.print(" y_raw="); Serial.print(p.y);
      Serial.print(" z="); Serial.println(p.z);

      delay(80); // debounce
    }
  }
}
