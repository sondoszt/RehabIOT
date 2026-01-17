#include "Shared.h"
#include "Game1_FollowLight.h"
#include "Game2_MemorySequence.h"
#include "Game3_ColorMatch.h"

// ---------- MENU UI ----------
static const uint16_t C_BG     = 0x08A3;
static const uint16_t C_PANEL2 = 0x18E7;
static const uint16_t C_MUTED  = 0xBDF7;
static const uint16_t C_ACCENT = 0x07FF;
static const uint16_t C_OK     = 0x07E0;
static const uint16_t C_BAD    = 0xF800;
static const uint16_t C_WARN   = 0xFD20;

static const int BTN_X = 20;
static const int BTN_W = 280;
static const int BTN_H = 42;
static const int BTN1_Y = 70;
static const int BTN2_Y = 122;
static const int BTN3_Y = 174;

static uint16_t blend565(uint16_t c1, uint16_t c2, uint8_t t) {
  uint8_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  uint8_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  uint8_t r = (r1*(255-t) + r2*t)/255;
  uint8_t g = (g1*(255-t) + g2*t)/255;
  uint8_t b = (b1*(255-t) + b2*t)/255;
  return (r<<11) | (g<<5) | b;
}

static void fillGradV(int x, int y, int w, int h, uint16_t top, uint16_t bot, int step=2) {
  for(int i=0;i<h;i+=step){
    uint8_t t = (uint32_t)i * 255 / (h - 1);
    uint16_t c = blend565(top, bot, t);
    tft.fillRect(x, y+i, w, step, c);
  }
}

static void drawBackground() {
  fillGradV(0,0,SCREEN_W,SCREEN_H, C_BG, 0x0008, 2);
  for(int i=0;i<18;i++) tft.drawPixel(random(0,SCREEN_W), random(0,SCREEN_H), TFT_WHITE);
}

static void uiButton(int x,int y,int w,int h,uint16_t bg,const char* label) {
  tft.fillRoundRect(x+3,y+3,w,h,16,TFT_BLACK);
  tft.fillRoundRect(x,y,w,h,16,bg);
  tft.drawRoundRect(x,y,w,h,16,TFT_WHITE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_BLACK, bg);
  tft.drawString(label, x+w/2+1, y+h/2+1);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(label, x+w/2, y+h/2);
  tft.setTextDatum(TL_DATUM);
}

// ✅ IMPORTANT: not static anymore (so other files can call it via goMenu)
void drawMenu() {
  strip.clear(); strip.show();
  drawBackground();

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(C_ACCENT, TFT_BLACK);
  tft.drawString("RehabGames", 160, 28);

  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, TFT_BLACK);
  tft.drawString("Tap a game to start", 160, 50);
  tft.setTextDatum(TL_DATUM);

  uiButton(BTN_X, BTN1_Y, BTN_W, BTN_H, C_OK,   "FOLLOW THE LIGHT");
  uiButton(BTN_X, BTN2_Y, BTN_W, BTN_H, C_WARN, "LIGHT SEQUENCE");
  uiButton(BTN_X, BTN3_Y, BTN_W, BTN_H, C_BAD,  "COLOR MATCH");
}

// ✅ Global navigation helpers (games can call these)
void goMenu() {
  g_screen = SCR_MENU;
  drawMenu();
}

void goGame(AppScreen s) {
  g_screen = s;
  if (s == SCR_GAME1) Game1_begin();
  else if (s == SCR_GAME2) Game2_begin();
  else if (s == SCR_GAME3) Game3_begin();
}

void setup() {
  Serial.begin(115200);
  delay(150);
  randomSeed(micros());

  Shared_setupHardware();

  goMenu();
}

void loop() {
  int sx, sy;

  if(g_screen == SCR_MENU) {
    if(Touch_pressed(sx, sy)) {
      if      (inRect(sx,sy, BTN_X,BTN1_Y,BTN_W,BTN_H)) goGame(SCR_GAME1);
      else if (inRect(sx,sy, BTN_X,BTN2_Y,BTN_W,BTN_H)) goGame(SCR_GAME2);
      else if (inRect(sx,sy, BTN_X,BTN3_Y,BTN_W,BTN_H)) goGame(SCR_GAME3);
    }
    delay(10);
    return;
  }

  if(g_screen == SCR_GAME1) { Game1_update(); return; }
  if(g_screen == SCR_GAME2) { Game2_update(); return; }
  if(g_screen == SCR_GAME3) { Game3_update(); return; }

  delay(10);
}
