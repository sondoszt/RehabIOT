#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#define TFT_CS     15   // TFT chip select (disable it)
#define CS_PIN      5   // TOUCH T_CS
#define TIRQ_PIN   27   // TOUCH T_IRQ (put 255 if not connected)

XPT2046_Touchscreen ts(CS_PIN, TIRQ_PIN);

void setup() {
  Serial.begin(115200);
  delay(300);

  // Disable TFT so it won't interfere on SPI
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

  // Use your ESP32 SPI pins
  SPI.begin(18, 19, 23);     // SCK, MISO, MOSI
  SPI.setFrequency(1000000); // 1 MHz (stable)

  ts.begin();
  ts.setRotation(1);

  Serial.println("XPT2046 test ready. Touch the screen...");
}

void loop() {
  // If IRQ is wired correctly, touched() should work.
  // If not, still try reading when IRQ goes LOW:
  if (digitalRead(TIRQ_PIN) == LOW || ts.touched()) {
    TS_Point p = ts.getPoint();
    Serial.print("Pressure=");
    Serial.print(p.z);
    Serial.print(" x=");
    Serial.print(p.x);
    Serial.print(" y=");
    Serial.println(p.y);
    delay(80);
  }
}
