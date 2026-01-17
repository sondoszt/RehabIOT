// Menu.cpp
#include "Shared.h"

// ---------- Layout ----------
static const int BTN_X = 30;
static const int BTN_W = 260;
static const int BTN_H = 44;

static const int BTN1_Y = 70;
static const int BTN2_Y = 125;
static const int BTN3_Y = 180;

// ---------- Simple gradient background ----------
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
  // same vibe as your other screens
  const uint16_t C_BG_TOP = 0x08A3;
  const uint16_t C_BG_BOT = 0x0008;

  fillGradV(0, 0, SCREEN_W, SCREEN_H, C_BG_TOP, C_BG_BOT, 2);

  // tiny stars
  for(int i=0;i<22;i++){
    tft.drawPixel(random(0, SCREEN_W), random(0, SCREEN_H), TFT_WHITE);
  }
}

static void drawButton(int x,int y,int w,int h,uint16_t bg,const char* label){
  tft.fillRoundRect(x+3,y+3,w,h,14,TFT_BLACK);
  tft.fillRoundRect(x,y,w,h,14,bg);
  tft.drawRoundRect(x,y,w,h,14,TFT_WHITE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);

  // small shadow
  tft.setTextColor(TFT_BLACK, bg);
  tft.drawString(label, x+w/2+1, y+h/2+1);

  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(label, x+w/2, y+h/2);

  tft.setTextDatum(TL_DATUM);
}

// goGame() is implemented in your main .ino
extern void goGame(AppScreen s);

void Menu_draw() {
  strip.clear();
  strip.show();

  randomSeed(micros());
  drawBackground();

  // Title
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(0x07FF, TFT_BLACK);   // cyan-ish
  tft.drawString("RehabGames", 160, 26);

  // Subtitle
  tft.setTextFont(2);
  tft.setTextColor(0xBDF7, TFT_BLACK);
  tft.drawString("Tap a game to start", 160, 50);

  tft.setTextDatum(TL_DATUM);

  // Buttons
  drawButton(BTN_X, BTN1_Y, BTN_W, BTN_H, 0x07E0, "FOLLOW THE LIGHT");
  drawButton(BTN_X, BTN2_Y, BTN_W, BTN_H, 0xFD20, "LIGHT SEQUENCE");
  drawButton(BTN_X, BTN3_Y, BTN_W, BTN_H, 0xF800, "COLOR MATCH");
}

void Menu_update() {
  int x, y;
  if(!Touch_pressed(x,y)) return;

  if(inRect(x,y, BTN_X,BTN1_Y,BTN_W,BTN_H))      goGame(SCR_GAME1);
  else if(inRect(x,y, BTN_X,BTN2_Y,BTN_W,BTN_H)) goGame(SCR_GAME2);
  else if(inRect(x,y, BTN_X,BTN3_Y,BTN_W,BTN_H)) goGame(SCR_GAME3);
}
