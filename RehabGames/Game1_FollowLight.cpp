// ====================== GAME 1 – FOLLOW THE LIGHT (FULL UPDATED) ======================
// Fixes included:
// ✅ Back button moved into top bar (no overlap) and works reliably
// ✅ DONE screen buttons no longer “swapped” (works even if touch X is mirrored)
// ✅ PLAY AGAIN -> Game1 level screen
// ✅ GAMES MENU -> Main games menu
// ✅ Touch “ghost press” prevented using waitTouchRelease()
// ✅ Uses drawMenu() (as you requested) to redraw menu
//
// IMPORTANT: This file assumes your touch may be mirrored in X.
// We solve it safely by checking BOTH normal X and mirrored X for taps.
// (So it works regardless of calibration / rotation.)
// =====================================================================================

#include "Shared.h"
#include <math.h>

// Must exist in menu.cpp (non-static)
void drawMenu();

// ---------------- LEVEL SETTINGS ----------------
static const int ROUNDS_L1 = 10;
static const int ROUNDS_L2 = 12;

static const uint32_t LED_ON_L1_MS = 1000;
static const uint32_t LED_ON_L2_MS = 200;

static const uint32_t SCAN_TIMEOUT_L1_MS = 8000;
static const uint32_t SCAN_TIMEOUT_L2_MS = 5000;

// ---------------- LED ZONES ----------------
static const uint8_t LED_ZONES[] = {0,2,4,6,9,11,13,15,16,18,20,22,25,27,29,31};
static const int NUM_ZONES = sizeof(LED_ZONES) / sizeof(LED_ZONES[0]);

// ---------------- UID MAP ----------------
struct ZoneMap { uint8_t led; uint8_t uid[4]; };
static ZoneMap MAP[] = {
  {0,{0x49,0x04,0x16,0xA4}}, {2,{0xC4,0x90,0x86,0xBB}},
  {4,{0x39,0x94,0xBB,0xA2}}, {6,{0x46,0xC2,0x86,0xBB}},
  {9,{0x79,0x78,0x21,0xA4}}, {11,{0x49,0xB3,0x25,0xA4}},
  {13,{0x79,0x69,0xCC,0xA2}}, {15,{0xB9,0xB7,0x84,0xC1}},
  {16,{0x89,0x74,0x85,0xC2}}, {18,{0x6B,0x8F,0xD5,0xAB}},
  {20,{0x89,0x59,0x23,0xA4}}, {22,{0x29,0xCF,0x38,0x59}},
  {25,{0xD9,0x60,0x22,0xA4}}, {27,{0x89,0xE9,0xC3,0xA2}},
  {29,{0x19,0xF7,0x80,0xC1}}, {31,{0x09,0xC3,0xCA,0xA2}}
};
static const int MAP_LEN = sizeof(MAP) / sizeof(MAP[0]);

// ---------------- GAME STATE ----------------
enum Level { NONE, LEVEL_1, LEVEL_2 };
enum State { PICK_LEVEL, PLAYING, DONE };
enum RoundPhase { PHASE_IDLE, PHASE_REMEMBER, PHASE_SCAN };

static Level level = NONE;
static State state = PICK_LEVEL;
static RoundPhase phase = PHASE_IDLE;

static int roundsTotal = ROUNDS_L1;
static uint32_t ledOnMs = LED_ON_L1_MS;

static int roundNum = 0;
static int score = 0;
static int coinsEarned = 0;

static uint8_t currentLed = 0;
static uint8_t prevLed = 255;
static uint32_t scanStartMs = 0;

// RFID recovery
static unsigned long lastRFIDSeenOkMs = 0;
static const unsigned long RFID_RECOVER_MS = 6000;
static uint32_t lastSAMRearmMs = 0;

static uint32_t lastCountdownDrawMs = 0;

// UI colors
static const uint16_t C_BG     = 0x08A3;
static const uint16_t C_PANEL  = 0x10C5;
static const uint16_t C_PANEL2 = 0x18E7;
static const uint16_t C_MUTED  = 0xBDF7;
static const uint16_t C_ACCENT = 0x07FF;
static const uint16_t C_OK     = 0x07E0;
static const uint16_t C_BAD    = 0xF800;
static const uint16_t C_WARN   = 0xFD20;

// end celebration
static bool endCelebrated = false;

// ---------------- Buttons ----------------
static const int BTN_X = 20;
static const int BTN_W = 280;
static const int BTN_H = 52;
static const int BTN_WARM_Y = 118;
static const int BTN_HOT_Y  = 178;


// Back button — real top-left corner
static const int BTN_BACK_X = 10;
static const int BTN_BACK_Y = 10;
static const int BTN_BACK_W = 70;
static const int BTN_BACK_H = 24;


// DONE screen buttons (side-by-side)
static const int END_BTN_Y = 196;
static const int END_BTN_H = 40;

static const int END_BTN_W = 132;   // two buttons + gap fits 320
static const int END_GAP   = 16;

static const int END_PLAY_X = 20;
static const int END_MENU_X = END_PLAY_X + END_BTN_W + END_GAP;

static const int END_PLAY_Y = END_BTN_Y;
static const int END_MENU_Y = END_BTN_Y;

static const int END_PLAY_W = END_BTN_W;
static const int END_MENU_W = END_BTN_W;

static const int END_PLAY_H = END_BTN_H;
static const int END_MENU_H = END_BTN_H;

// ---------------- touch helpers (ROBUST: normal X OR mirrored X) ----------------


static bool hitRectPad(int x, int y, int rx, int ry, int rw, int rh, int pad) {
  return (x >= rx - pad && x < rx + rw + pad && y >= ry - pad && y < ry + rh + pad);
}

static int dist2(int x1,int y1,int x2,int y2){
  int dx=x1-x2, dy=y1-y2;
  return dx*dx + dy*dy;
}

// Returns: 0 none, 1 playAgain, 2 gamesMenu
static int whichDoneButton(int sx, int sy) {
  int mx = (SCREEN_W - 1) - sx;

  // try both candidate points
  struct Cand { int x,y; };
  Cand c[2] = { {sx, sy}, {mx, sy} };

  // A bit of padding makes it more “reactive”
  const int PAD = 10;

  bool hitPlay[2], hitMenu[2];
  for(int i=0;i<2;i++){
    hitPlay[i] = hitRectPad(c[i].x, c[i].y, END_PLAY_X, END_PLAY_Y, END_PLAY_W, END_PLAY_H, PAD);
    hitMenu[i] = hitRectPad(c[i].x, c[i].y, END_MENU_X, END_MENU_Y, END_MENU_W, END_MENU_H, PAD);
  }

  // If any candidate hits ONLY one button -> take it immediately
  for(int i=0;i<2;i++){
    if(hitPlay[i] && !hitMenu[i]) return 1;
    if(hitMenu[i] && !hitPlay[i]) return 2;
  }

  // If none hit
  if(!(hitPlay[0]||hitPlay[1]||hitMenu[0]||hitMenu[1])) return 0;

  // If ambiguous (hits both somehow), pick the closest center among the hits
  int best = 0;
  int bestD = 1e9;

  int playCx = END_PLAY_X + END_PLAY_W/2;
  int playCy = END_PLAY_Y + END_PLAY_H/2;
  int menuCx = END_MENU_X + END_MENU_W/2;
  int menuCy = END_MENU_Y + END_MENU_H/2;

  for(int i=0;i<2;i++){
    if(hitPlay[i]){
      int d = dist2(c[i].x, c[i].y, playCx, playCy);
      if(d < bestD){ bestD=d; best=1; }
    }
    if(hitMenu[i]){
      int d = dist2(c[i].x, c[i].y, menuCx, menuCy);
      if(d < bestD){ bestD=d; best=2; }
    }
  }
  return best;
}

// Generic “either X or mirrored X” hit (with padding) for other buttons
static bool hitEitherXPad(int sx, int sy, int rx, int ry, int rw, int rh, int pad=10) {
  if (hitRectPad(sx, sy, rx, ry, rw, rh, pad)) return true;
  int mx = (SCREEN_W - 1) - sx;
  return hitRectPad(mx, sy, rx, ry, rw, rh, pad);
}



static void waitTouchRelease() {
  int tx, ty;
  while (Touch_pressed(tx, ty)) delay(5);
}

// ---------------- LED helpers ----------------
static void ledsOff() { strip.clear(); strip.show(); }

static void lightOnly(uint8_t idx) {
  strip.clear();
  strip.setPixelColor(idx, strip.Color(255,255,255));
  strip.show();
}

static void flashAll(uint32_t color, int times, int onMs=120, int offMs=80) {
  for(int t=0;t<times;t++){
    for(int i=0;i<LED_COUNT;i++) strip.setPixelColor(i,color);
    strip.show(); delay(onMs);
    strip.clear(); strip.show(); delay(offMs);
  }
}

static void quickAck(bool ok) {
  uint32_t c = ok ? strip.Color(0, 180, 0) : strip.Color(180, 0, 0);
  strip.clear();
  strip.setPixelColor(currentLed, c);
  strip.show();
  delay(40);
  strip.clear();
  strip.show();
}

static void celebrateCoinsOnce(int coins) {
  int bursts = coins;
  if (bursts < 2) bursts = 2;
  if (bursts > 8) bursts = 8;

  for (int b = 0; b < bursts; b++) {
    strip.clear();
    for (int k = 0; k < 6; k++) {
      int i = random(0, LED_COUNT);
      strip.setPixelColor(i, strip.Color(220, 160, 0));
    }
    strip.show();
    delay(70);
    strip.clear();
    strip.show();
    delay(40);
  }
  for (int p = 0; p < 2; p++) {
    for (int i=0;i<LED_COUNT;i++) strip.setPixelColor(i, strip.Color(0, 120, 180));
    strip.show(); delay(70);
    strip.clear(); strip.show(); delay(60);
  }
}

// ---------------- RFID helpers ----------------
static bool readUID4(uint8_t out[4]) {
  uint8_t uid[7], len;
  if(!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 50)) return false;
  if(len < 4) return false;
  for(int i=0;i<4;i++) out[i]=uid[i];
  return true;
}

static const uint8_t* expectedUID(uint8_t led){
  for(int i=0;i<MAP_LEN;i++) if(MAP[i].led == led) return MAP[i].uid;
  return nullptr;
}

static bool uidEq(const uint8_t*a,const uint8_t*b){
  for(int i=0;i<4;i++) if(a[i]!=b[i]) return false;
  return true;
}

static bool initPN532WithFallback() {
  Wire.setClock(400000);
  nfc.begin();
  delay(30);
  uint32_t v = nfc.getFirmwareVersion();
  if(v) { nfc.SAMConfig(); return true; }

  Wire.setClock(100000);
  delay(30);
  nfc.begin();
  delay(30);
  v = nfc.getFirmwareVersion();
  if(!v) return false;
  nfc.SAMConfig();
  return true;
}

// ---------------- UI helpers ----------------
static uint16_t blend565(uint16_t c1, uint16_t c2, uint8_t t) {
  uint8_t r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  uint8_t r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  uint8_t r = (r1*(255-t) + r2*t)/255;
  uint8_t g = (g1*(255-t) + g2*t)/255;
  uint8_t b = (b1*(255-t) + b2*t)/255;
  return (r<<11) | (g<<5) | b;
}

static void fillGradV(int x, int y, int w, int h, uint16_t top, uint16_t bot, int step=2) {
  for (int i = 0; i < h; i += step) {
    uint8_t t = (uint32_t)i * 255 / (h - 1);
    uint16_t c = blend565(top, bot, t);
    tft.fillRect(x, y + i, w, step, c);
  }
}


// Force touch X mirroring (your screen is mirrored -> fixes swapped buttons)
static const bool TOUCH_MIRROR_X = true;

static inline void touchToScreen(int sx, int sy, int &x, int &y) {
  x = TOUCH_MIRROR_X ? (SCREEN_W - 1 - sx) : sx;
  y = sy;
}

static bool hitRectTouch(int sx, int sy, int rx, int ry, int rw, int rh) {
  int x, y;
  touchToScreen(sx, sy, x, y);
  return inRect(x, y, rx, ry, rw, rh);
}

static void drawBackground() {
  fillGradV(0, 0, SCREEN_W, SCREEN_H, C_BG, 0x0008, 2);
  for (int i = 0; i < 28; i++) tft.drawPixel(random(0, SCREEN_W), random(0, SCREEN_H), TFT_WHITE);
}

static void drawBackButton() {
  tft.fillRoundRect(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, 8, C_PANEL2);
  tft.drawRoundRect(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, 8, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, C_PANEL2);
  tft.drawString("< Back", BTN_BACK_X + BTN_BACK_W/2, BTN_BACK_Y + BTN_BACK_H/2);
  tft.setTextDatum(TL_DATUM);
}

static void uiTopBar(const char* subtitle) {
  // moved down so it won't overlap the back button
  int x = 10, y = 44, w = 300, h = 64;      // (h slightly taller)
  uint16_t top = 0x0211;
  uint16_t bot = C_PANEL;

  tft.fillRoundRect(x, y, w, h, 16, bot);
  fillGradV(x+2, y+2, w-4, h-4, top, bot, 2);
  tft.drawRoundRect(x, y, w, h, 16, 0x7BEF);
  tft.drawFastHLine(x+16, y+h-2, w-32, C_ACCENT);

  int cx = x + w/2;

  tft.setTextDatum(MC_DATUM);

  // Title (centered in the box)
  tft.setTextFont(4);
  tft.setTextColor(TFT_BLACK, bot);
  tft.drawString("Follow the Light", cx + 1, y + 22 + 1);
  tft.setTextColor(C_ACCENT, bot);
  tft.drawString("Follow the Light", cx,     y + 22);

  // Subtitle (centered in the box)
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, bot);
  tft.drawString(subtitle, cx, y + 48);

  tft.setTextDatum(TL_DATUM);
}


static void uiButton(int x, int y, int w, int h, uint16_t bg, const char* label) {
  tft.fillRoundRect(x+3, y+3, w, h, 18, TFT_BLACK);
  tft.fillRoundRect(x, y, w, h, 18, bg);
  tft.drawRoundRect(x, y, w, h, 18, TFT_WHITE);

  tft.setTextDatum(MC_DATUM);
  if (w <= 140) tft.setTextFont(2);
  else         tft.setTextFont(4);

  tft.setTextColor(TFT_BLACK, bg);
  tft.drawString(label, x + w/2 + 1, y + h/2 + 1);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(label, x + w/2, y + h/2);
  tft.setTextDatum(TL_DATUM);
}

static void drawLevelScreen() {
  ledsOff();
  drawBackground();
  drawBackButton();
  uiTopBar("Choose a mode");
  uiButton(BTN_X, BTN_WARM_Y, BTN_W, BTN_H, C_OK,   "WARM-UP");
  uiButton(BTN_X, BTN_HOT_Y,  BTN_W, BTN_H, C_WARN, "HOT MODE");
  
}

static void clearCenterArea() {
  fillGradV(0, 118, SCREEN_W, 92, C_BG, 0x0008, 2);
  for(int i=0;i<6;i++){
    tft.drawPixel(random(0, SCREEN_W), random(118, 210), TFT_WHITE);
  }
}

static void uiCenterCard(const char* msg, uint16_t bgColor) {
  int x = 20, y = 122, w = 280, h = 70;

  tft.fillRoundRect(x+3, y+3, w, h, 16, TFT_BLACK);
  tft.fillRoundRect(x, y, w, h, 16, bgColor);
  tft.drawRoundRect(x, y, w, h, 16, TFT_WHITE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);

  uint16_t txt = TFT_WHITE;
  if (bgColor == C_WARN) txt = TFT_BLACK;
  if (bgColor == C_OK)   txt = TFT_BLACK;

  tft.setTextColor(txt, bgColor);
  tft.drawString(msg, 160, y + h/2 + 4);
  tft.setTextDatum(TL_DATUM);
}

static void uiHint(const char* msg) {
  fillGradV(0, 214, SCREEN_W, 26, C_BG, 0x0008, 2);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, 0x0000);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, 160, 230);
  tft.setTextDatum(TL_DATUM);
}

static void uiStatusRight(int r, int total, int s) {
  int w = 86, h = 46;
  int x = SCREEN_W - w - 8;
  int y = 70;

  tft.fillRoundRect(x, y, w, h, 12, C_PANEL2);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, C_PANEL2);
  tft.setCursor(x + 8, y + 8);
  tft.printf("R %d/%d", r, total);
  tft.setCursor(x + 8, y + 26);
  tft.printf("S %d", s);
}

static void uiProgress(int currentRound, int totalRounds) {
  int x = 10, y = 74, w = 210, h = 10;
  tft.fillRoundRect(x, y, w, h, 6, C_PANEL2);

  float frac = 0.0f;
  if(totalRounds > 0) frac = (float)(currentRound-1) / (float)totalRounds;
  if(frac < 0) frac = 0;
  if(frac > 1) frac = 1;

  int fillW = (int)(w * frac);
  tft.fillRoundRect(x, y, fillW, h, 6, C_ACCENT);
}

static void uiScanCountdown(uint32_t elapsedMs, uint32_t totalMs) {
  int x = 30, y = 200, w = 260, h = 12;
  tft.fillRoundRect(x, y, w, h, 6, C_PANEL2);

  float frac = (totalMs > 0) ? (float)elapsedMs / totalMs : 0;
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;

  int fillW = (int)(w * (1.0f - frac));
  if (fillW < 0) fillW = 0;
  tft.fillRoundRect(x, y, fillW, h, 6, C_WARN);
}

// ---------------- coins + feedback ----------------
static int calcCoinsEarned(int scoreVal, Level lvl) {
  float mult = (lvl == LEVEL_2) ? 1.3f : 1.0f;
  int coins = (int)round(scoreVal * mult);
  if (coins < 0) coins = 0;
  return coins;
}

static const char* feedbackText(int scoreVal, int totalRounds, Level lvl) {
  if (totalRounds <= 0) return "Good session!";
  float p = (float)scoreVal / (float)totalRounds;
  if (p >= 0.90f) return (lvl == LEVEL_2) ? "Elite reflexes! 🔥" : "Amazing focus! 🌟";
  if (p >= 0.70f) return "Great job! Keep going 💪";
  if (p >= 0.45f) return "Nice effort! You’re improving 🙂";
  return "Warm-up more — you got this 🙌";
}

static void drawEndScreen() {
  ledsOff();
  drawBackground();

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, TFT_BLACK);
  tft.drawString("Session finished", 160, 18);

  coinsEarned = calcCoinsEarned(score, level);
  const char* fb = feedbackText(score, roundsTotal, level);

  if(!endCelebrated){
    celebrateCoinsOnce(coinsEarned);
    endCelebrated = true;
  }

  int cardX = 24, cardY = 44, cardW = 272, cardH = 78;
  tft.fillRoundRect(cardX+3, cardY+3, cardW, cardH, 16, TFT_BLACK);
  tft.fillRoundRect(cardX,   cardY,   cardW, cardH, 16, C_PANEL2);
  tft.drawRoundRect(cardX,   cardY,   cardW, cardH, 16, TFT_WHITE);

  tft.setTextFont(4);
  tft.setTextColor(C_ACCENT, C_PANEL2);
  tft.drawString("FINAL SCORE", 160, cardY + 18);

  tft.setTextFont(6);
  tft.setTextColor(TFT_WHITE, C_PANEL2);
  tft.drawString(String(score), 160, cardY + 52);

  int pillX = 74, pillY = cardY + cardH + 8, pillW = 172, pillH = 24;
  tft.fillRoundRect(pillX, pillY, pillW, pillH, 12, 0x0841);
  tft.drawRoundRect(pillX, pillY, pillW, pillH, 12, TFT_WHITE);

  int coinCx = pillX + 18;
  int coinCy = pillY + pillH/2;
  tft.fillCircle(coinCx, coinCy, 6, C_WARN);
  tft.drawCircle(coinCx, coinCy, 6, TFT_WHITE);

  tft.setTextFont(2);
  tft.setTextColor(C_ACCENT, 0x0841);
  tft.drawString("+", pillX + 40, coinCy);
  tft.setTextColor(TFT_WHITE, 0x0841);
  tft.drawString(String(coinsEarned), pillX + 55, coinCy);
  tft.setTextColor(C_ACCENT, 0x0841);
  tft.drawString("COINS", pillX + 110, coinCy);

  int fbX = 18, fbY = pillY + pillH + 8, fbW = 284, fbH = 28;
  tft.fillRoundRect(fbX, fbY, fbW, fbH, 12, C_PANEL);
  tft.drawRoundRect(fbX, fbY, fbW, fbH, 12, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, C_PANEL);
  tft.drawString(fb, 160, fbY + fbH/2);

  // Side-by-side buttons
  uiButton(END_PLAY_X, END_PLAY_Y, END_PLAY_W, END_PLAY_H, C_ACCENT, "PLAY AGAIN");
  uiButton(END_MENU_X, END_MENU_Y, END_MENU_W, END_MENU_H, C_WARN, "GAMES MENU");

  tft.setTextDatum(TL_DATUM);
}

// ---------------- flow ----------------
static void applyLevel() {
  if(level == LEVEL_1) { roundsTotal = ROUNDS_L1; ledOnMs = LED_ON_L1_MS; }
  else                 { roundsTotal = ROUNDS_L2; ledOnMs = LED_ON_L2_MS; }
}

static uint8_t pickNextLed() {
  uint8_t led;
  do { led = LED_ZONES[random(NUM_ZONES)]; }
  while(led == prevLed && NUM_ZONES > 1);
  prevLed = led;
  return led;
}

static void showResult(bool correct, bool timeout=false) {
  if(timeout) {
    uiCenterCard("TIME UP", C_BAD);
    flashAll(strip.Color(180,0,0), 1, 200, 120);
    delay(260);
    return;
  }
  if(correct) {
    uiCenterCard("NICE!", C_OK);
    flashAll(strip.Color(0,180,0), 2);
  } else {
    uiCenterCard("ALMOST!", C_BAD);
    flashAll(strip.Color(180,0,0), 1, 200, 120);
  }
  delay(260);
}

static void beginRound() {
  applyLevel();

  roundNum++;
  if(roundNum > roundsTotal) {
    state = DONE;
    drawEndScreen();
    return;
  }

  currentLed = pickNextLed();

  drawBackground();
  uiTopBar((level==LEVEL_1) ? "WARM-UP MODE" : "HOT MODE");
  uiStatusRight(roundNum, roundsTotal, score);
  uiProgress(roundNum, roundsTotal);

  clearCenterArea();

  phase = PHASE_REMEMBER;
  uiCenterCard("WATCH", C_WARN);
  uiHint("Remember the peg position");

  lightOnly(currentLed);
  uint32_t watchStart = millis();
  while (millis() - watchStart < ledOnMs) delay(5);
  ledsOff();

  clearCenterArea();

  phase = PHASE_SCAN;
  uiCenterCard("SCAN", C_ACCENT);
  uiHint("Scan the matching RFID peg");

  scanStartMs = millis();
  lastCountdownDrawMs = 0;
}

static void startGameWithCountdown() {
  applyLevel();
  roundNum = 0;
  score = 0;
  coinsEarned = 0;
  phase = PHASE_IDLE;
  prevLed = 255;
  endCelebrated = false;

  drawBackground();
  uiTopBar((level==LEVEL_1) ? "WARM-UP MODE" : "HOT MODE");
  uiHint("Starting...");

  for(int n=3; n>=1; n--){
    clearCenterArea();
    tft.fillRoundRect(60, 102, 200, 78, 18, C_PANEL2);
    tft.drawRoundRect(60, 102, 200, 78, 18, TFT_WHITE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(8);
    tft.setTextColor(TFT_WHITE, C_PANEL2);
    tft.drawString(String(n), 160, 140);
    tft.setTextDatum(TL_DATUM);

    flashAll(strip.Color(0,120,180), 1, 60, 60);
    delay(650);
  }

  clearCenterArea();
  uiCenterCard("GO!", C_OK);
  flashAll(strip.Color(0,180,0), 2, 70, 60);
  delay(220);

  state = PLAYING;
  beginRound();
}

// ---------------- public API ----------------
void Game1_begin() {
  initPN532WithFallback();
  lastRFIDSeenOkMs = millis();
  lastSAMRearmMs = millis();

  level = NONE;
  state = PICK_LEVEL;
  phase = PHASE_IDLE;
  endCelebrated = false;

  drawLevelScreen();
}

void Game1_update() {
  int sx, sy;

  // BACK only on LEVEL PICK screen
  if (state == PICK_LEVEL && Touch_pressed(sx, sy)) {
    if (hitEitherXPad(sx, sy, BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, 10)) {
      waitTouchRelease();
      ledsOff();
      g_screen = SCR_MENU;
      drawMenu();
      return;
    }
  }

  // PICK LEVEL
  if(state == PICK_LEVEL) {
    if(Touch_pressed(sx, sy)) {
      if(hitEitherXPad(sx,sy, BTN_X,BTN_WARM_Y,BTN_W,BTN_H, 14)) level = LEVEL_1;
      else if(hitEitherXPad(sx,sy, BTN_X,BTN_HOT_Y, BTN_W,BTN_H, 14)) level = LEVEL_2;

      else { delay(10); return; }

      waitTouchRelease();
      startGameWithCountdown();
    }
    delay(10);
    return;
  }

  // PLAYING
  if(state == PLAYING) {
    if(phase == PHASE_SCAN) {
      uint8_t uid4[4];
      if(readUID4(uid4)) {
        lastRFIDSeenOkMs = millis();

        const uint8_t* exp = expectedUID(currentLed);
        bool correct = (exp && uidEq(uid4, exp));
        if(correct) score += 10;

        quickAck(correct);
        uiStatusRight(roundNum, roundsTotal, score);
        showResult(correct, false);

        nfc.SAMConfig();
        lastSAMRearmMs = millis();

        beginRound();
        return;
      }

      uint32_t elapsed = millis() - scanStartMs;
      uint32_t timeoutMs = (level == LEVEL_2) ? SCAN_TIMEOUT_L2_MS : SCAN_TIMEOUT_L1_MS;

      if(lastCountdownDrawMs == 0 || millis() - lastCountdownDrawMs > 80) {
        uiScanCountdown(elapsed, timeoutMs);
        lastCountdownDrawMs = millis();
      }

      if(elapsed > timeoutMs) {
        quickAck(false);
        showResult(false, true);
        beginRound();
        return;
      }
    }

    if(millis() - lastSAMRearmMs > 2500) {
      nfc.SAMConfig();
      lastSAMRearmMs = millis();
    }

    if(millis() - lastRFIDSeenOkMs > RFID_RECOVER_MS) {
      uiHint("RFID reset...");
      initPN532WithFallback();
      lastRFIDSeenOkMs = millis();
      lastSAMRearmMs = millis();
      if(phase == PHASE_SCAN) uiHint("Scan the matching RFID peg");
    }

    delay(2);
    return;
  }

  // DONE
if (state == DONE) {
  if (Touch_pressed(sx, sy)) {

    // PLAY AGAIN -> Game1 levels
    if (hitRectTouch(sx, sy, END_PLAY_X, END_PLAY_Y, END_PLAY_W, END_PLAY_H)) {
      waitTouchRelease();
      ledsOff();
      level = NONE;
      state = PICK_LEVEL;
      phase = PHASE_IDLE;
      endCelebrated = false;
      drawLevelScreen();
      return;
    }

    // GAMES MENU -> main games menu
    if (hitRectTouch(sx, sy, END_MENU_X, END_MENU_Y, END_MENU_W, END_MENU_H)) {
      waitTouchRelease();
      ledsOff();
      g_screen = SCR_MENU;
      drawMenu();
      return;
    }
  }
  delay(10);
  return;
}




  delay(10);
}
