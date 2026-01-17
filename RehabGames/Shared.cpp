#include "Shared.h"

// --------- global objects (single instance) ----------
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
Adafruit_PN532 nfc(-1, -1); // I2C
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// --------- globals ----------
AppScreen g_screen = SCR_MENU;

// ✅ Put YOUR calibration here
int  TOUCH_X_MIN = 350;
int  TOUCH_X_MAX = 3800;
int  TOUCH_Y_MIN = 300;
int  TOUCH_Y_MAX = 3800;

// ✅ Toggle if your touch mapping is flipped
bool TOUCH_SWAP_XY  = false;
bool TOUCH_INVERT_X = true;
bool TOUCH_INVERT_Y = false;

static uint32_t lastTouchMs = 0;

static int clampi(int v,int lo,int hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }

bool inRect(int x,int y,int rx,int ry,int rw,int rh){
  return (x>=rx && x<=rx+rw && y>=ry && y<=ry+rh);
}

// ---- touch reading ----
static bool readTouchRawInternal(TS_Point &out){
  digitalWrite(TFT_CS_PIN, HIGH);

  if (TOUCH_IRQ != 255 && digitalRead(TOUCH_IRQ) == HIGH) return false;

  TS_Point best(0,0,0);
  for(int i=0;i<6;i++){
    TS_Point p = ts.getPoint();
    if(p.z > best.z) best = p;
    delay(2);
  }

  if(best.z < Z_MIN || best.z > Z_MAX) return false;
  out = best;
  return true;
}

static bool rawToScreenInternal(const TS_Point &raw, int &sx, int &sy){
  int rx = raw.x, ry = raw.y;
  if(TOUCH_SWAP_XY){ int tmp=rx; rx=ry; ry=tmp; }

  rx = clampi(rx, TOUCH_X_MIN, TOUCH_X_MAX);
  ry = clampi(ry, TOUCH_Y_MIN, TOUCH_Y_MAX);

  long nx = (long)(rx - TOUCH_X_MIN) * (SCREEN_W - 1) / (TOUCH_X_MAX - TOUCH_X_MIN);
  long ny = (long)(ry - TOUCH_Y_MIN) * (SCREEN_H - 1) / (TOUCH_Y_MAX - TOUCH_Y_MIN);

  if(TOUCH_INVERT_X) nx = (SCREEN_W - 1) - nx;
  if(TOUCH_INVERT_Y) ny = (SCREEN_H - 1) - ny;

  sx = (int)nx;
  sy = (int)ny;
  return true;
}

bool Touch_pressed(int &sx, int &sy){
  TS_Point raw;
  if(!readTouchRawInternal(raw)) return false;
  if(millis() - lastTouchMs < 220) return false;
  lastTouchMs = millis();
  return rawToScreenInternal(raw, sx, sy);
}

bool Touch_pressedRaw(int &sx, int &sy, TS_Point &raw){
  if(!readTouchRawInternal(raw)) return false;
  if(millis() - lastTouchMs < 220) return false;
  lastTouchMs = millis();
  rawToScreenInternal(raw, sx, sy);
  return true;
}


void Shared_setupHardware(){
  pinMode(TFT_CS_PIN, OUTPUT);
  digitalWrite(TFT_CS_PIN, HIGH);

  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);

  if(TOUCH_IRQ != 255) pinMode(TOUCH_IRQ, INPUT);

  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);

  tft.init();
  tft.setRotation(TFT_ROT);
  tft.setTextWrap(false);

  ts.begin();
  ts.setRotation(TS_ROT);

  strip.begin();
  strip.clear();
  strip.show();

  Wire.begin(PN532_SDA, PN532_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);

  nfc.begin();
  nfc.SAMConfig();
}
void Shared_touchTick() {
  // optional place to handle touch IRQ filtering / future features.
  // For now: nothing needed because Touch_pressed() already debounces.
}

