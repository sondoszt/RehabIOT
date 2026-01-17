#include "Game3_ColorMatch.h"
#include <string.h>

// ============================================================
//  GAME 3 – COLOR MATCH PAIRS (MODULE VERSION - NO setup/loop)
//  Uses Shared.h globals: tft, ts, nfc, strip, g_screen
// ============================================================

// Must exist in menu.cpp (non-static)
void Menu_draw();

// ---------------- UI COLORS (565) ----------------
static const uint16_t C_BG     = 0x08A3;
static const uint16_t C_PANEL2 = 0x18E7;
static const uint16_t C_MUTED  = 0xBDF7;
static const uint16_t C_ACCENT = 0x07FF; // cyan
static const uint16_t C_OK     = 0x07E0; // green
static const uint16_t C_BAD    = 0xF800; // red
static const uint16_t C_WARN   = 0xFD20; // gold

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

// ---------------- UNIQUE PALETTE (1 color per pair) ----------------
static const uint32_t PALETTE[] = {
  0x00FFD400, // Yellow
  0x00FF4FD8, // Pink
  0x009B4DFF, // Purple
  0x00005BFF, // Blue
  0x00FF7A00, // Orange
  0x0000D94A, // Green
  0x00FF1A1A, // Red
  0x0000E5FF  // Cyan
};
static const int PALETTE_LEN = sizeof(PALETTE) / sizeof(PALETTE[0]);

// ---------------- GAME STATE ----------------
enum Level { LV_NONE, LV_EASY, LV_MEDIUM, LV_HARD };
enum State { ST_RFID_RETRY, ST_PICK_LEVEL, ST_COUNTDOWN, ST_SHOW_BOARD, ST_PLAY, ST_DONE };
enum InputPhase { WAIT_FOR_TAG, WAIT_FOR_LIFT };

static Level level = LV_NONE;
static State state = ST_RFID_RETRY;
static InputPhase inputPhase = WAIT_FOR_TAG;

static int score = 0;
static int coinsTotal = 0;     // total over device run
static int pairsMatched = 0;

// --------- Level parameters ----------
static int boardPairs = 4;
static uint32_t roundMs = 18000;

// --------- Round timer ----------
static uint32_t roundStartMs = 0;

// --------- Touch/UI layout ----------
static const int CARD_X = 30;
static const int CARD_Y = 44;
static const int CARD_W = 260;
static const int CARD_H = 42;

static const int BTN_X = 20;
static const int BTN_W = 280;
static const int BTN_H = 36;
static const int BTN_EASY_Y = 96;
static const int BTN_MED_Y  = 140;
static const int BTN_HARD_Y = 184;

// DONE buttons (Game1 style)
static const int END_BTN_Y = 186;
static const int END_BTN_H = 40;
static const int END_BTN_W = 132;
static const int END_GAP   = 16;
static const int END_PLAY_X = 20;
static const int END_MENU_X = END_PLAY_X + END_BTN_W + END_GAP;


static const int BTN_RETRY_X = 80;
static const int BTN_RETRY_Y = 168;
static const int BTN_RETRY_W = 160;
static const int BTN_RETRY_H = 46;

static const int BTN_DONE_X = 60;
static const int BTN_DONE_Y = 192;
static const int BTN_DONE_W = 200;
static const int BTN_DONE_H = 44;

// Back button
static const int BTN_BACK_X = 10;
static const int BTN_BACK_Y = 6;
static const int BTN_BACK_W = 70;
static const int BTN_BACK_H = 24;

// --------- Global time bar ----------
static const int BAR_X = 40;
static const int BAR_Y = 30;
static const int BAR_W = 240;
static const int BAR_H = 10;
static uint32_t lastBarDrawMs = 0;
static int lastBarFill = -1;

// --------- RFID watchdog ----------
static uint32_t lastRFIDOkMs = 0;
static uint32_t lastRecoverMs = 0;
static const uint32_t RFID_STUCK_MS = 2500;
static const uint32_t RECOVER_COOLDOWN_MS = 1200;

// --------- Coins breakdown (per round) ----------
static int coinsRound = 0;
static int coinsFromMatches = 0;
static int coinsFromBonus = 0;

// ---------------- MATCH BOARD ----------------
static const int MAX_ACTIVE = 16;
static int activeCount = 0;
static uint8_t activeLed[MAX_ACTIVE];
static bool matched[MAX_ACTIVE];
static uint8_t colorId[MAX_ACTIVE];

// selection
static bool hasFirst = false;
static int firstIdx = -1;

// ---------- UID STABILITY FILTER ----------
static uint8_t stableUid[4];
static uint8_t stableCount = 0;
static uint32_t lastUidMs = 0;
static const uint32_t UID_STABLE_WINDOW = 120;
static const uint8_t UID_REQUIRED_HITS = 2;

// ---------------- helpers ----------------
static void ledsOff() { strip.clear(); strip.show(); }

static void fillAll(uint32_t c) {
  for(int i=0;i<LED_COUNT;i++) strip.setPixelColor(i, c);
  strip.show();
}

// ---- TOUCH ORIENTATION (Game3) ----
// If your touch is mirrored in X, keep this = 1
#define TOUCH_MIRROR_X 1

static inline int mapX(int sx){
  return TOUCH_MIRROR_X ? (SCREEN_W - 1 - sx) : sx;
}

static bool hitRectMapped(int sx,int sy,int rx,int ry,int rw,int rh){
  int x = mapX(sx);
  return inRect(x, sy, rx, ry, rw, rh);
}

static void waitTouchRelease() {
  int tx, ty;
  while (Touch_pressed(tx, ty)) delay(5);
}


static void flashAll(uint32_t c, int times, int onMs=120, int offMs=80) {
  for(int t=0;t<times;t++){
    fillAll(c); delay(onMs);
    ledsOff();  delay(offMs);
  }
}

static void blinkLed(uint8_t led, uint32_t c, int times=1, int onMs=110, int offMs=70) {
  for(int t=0;t<times;t++){
    strip.setPixelColor(led, c);
    strip.show();
    delay(onMs);
    strip.setPixelColor(led, 0);
    strip.show();
    delay(offMs);
  }
}

static bool ledInUse(uint8_t led, int n) {
  for(int i=0;i<n;i++) if(activeLed[i]==led) return true;
  return false;
}

// ---------------- RFID helpers ----------------
static const uint8_t* expectedUIDForLed(uint8_t led){
  for(int i=0;i<MAP_LEN;i++) if(MAP[i].led == led) return MAP[i].uid;
  return nullptr;
}

static bool uidEq(const uint8_t*a,const uint8_t*b){
  for(int i=0;i<4;i++) if(a[i]!=b[i]) return false;
  return true;
}

static void kickPN532() { nfc.SAMConfig(); }

static void recoverRFID() {
  if (millis() - lastRecoverMs < RECOVER_COOLDOWN_MS) return;
  lastRecoverMs = millis();

  nfc.SAMConfig();
  delay(10);

  Wire.end();
  delay(20);
  Wire.begin(PN532_SDA, PN532_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
  delay(20);

  nfc.begin();
  delay(30);
  nfc.getFirmwareVersion();
  nfc.SAMConfig();

  lastRFIDOkMs = millis();
}

static bool readUID4(uint8_t out[4]) {
  uint8_t uid[7], len;
  bool ok = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 40);
  if(!ok || len < 4) {
    stableCount = 0;
    return false;
  }

  uint32_t now = millis();
  if(stableCount == 0 || (now - lastUidMs) > UID_STABLE_WINDOW) {
    memcpy(stableUid, uid, 4);
    stableCount = 1;
    lastUidMs = now;
    return false;
  }

  if(memcmp(stableUid, uid, 4) == 0) {
    stableCount++;
    lastUidMs = now;
    if(stableCount >= UID_REQUIRED_HITS) {
      memcpy(out, stableUid, 4);
      stableCount = 0;
      lastRFIDOkMs = millis();
      return true;
    }
  } else {
    memcpy(stableUid, uid, 4);
    stableCount = 1;
    lastUidMs = now;
  }
  return false;
}

static int findActiveIndexByUID(const uint8_t uid4[4]) {
  for(int i=0;i<activeCount;i++){
    const uint8_t* exp = expectedUIDForLed(activeLed[i]);
    if(exp && uidEq(uid4, exp)) return i;
  }
  return -1;
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

static void drawTopTitle(const char* title) {
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, TFT_BLACK);
  tft.drawString(title, 160, 18);
  tft.setTextDatum(TL_DATUM);
}

static void uiButton(int x,int y,int w,int h,uint16_t bg,const char* label) {
  tft.fillRoundRect(x+3,y+3,w,h,16,TFT_BLACK);
  tft.fillRoundRect(x,y,w,h,16,bg);
  tft.drawRoundRect(x,y,w,h,16,TFT_WHITE);

  tft.setTextDatum(MC_DATUM);

  // ✅ shrink font for narrow buttons (like PLAY AGAIN / GAMES MENU)
  if (w <= 140) tft.setTextFont(2);
  else          tft.setTextFont(4);

  tft.setTextColor(TFT_BLACK, bg);
  tft.drawString(label, x+w/2+1, y+h/2+1);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(label, x+w/2, y+h/2);

  tft.setTextDatum(TL_DATUM);
}


static void drawCenterCard(const char* top, const char* bottom) {
  tft.fillRoundRect(CARD_X+3, CARD_Y+3, CARD_W, CARD_H, 14, TFT_BLACK);
  tft.fillRoundRect(CARD_X,   CARD_Y,   CARD_W, CARD_H, 14, C_PANEL2);
  tft.drawRoundRect(CARD_X,   CARD_Y,   CARD_W, CARD_H, 14, TFT_WHITE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_ACCENT, C_PANEL2);
  tft.drawString(top, 160, CARD_Y + 14);

  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, C_PANEL2);
  tft.drawString(bottom, 160, CARD_Y + 28);
  tft.setTextDatum(TL_DATUM);
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

// --------- Time bar ----------
static void drawTimeBarFrame() {
  tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, 4, TFT_WHITE);
  tft.fillRoundRect(BAR_X+1, BAR_Y+1, BAR_W-2, BAR_H-2, 3, TFT_BLACK);
  lastBarFill = -1;
  lastBarDrawMs = 0;
}

static void updateTimeBar() {
  if(state != ST_PLAY) return;
  if(millis() - lastBarDrawMs < 80) return;
  lastBarDrawMs = millis();

  uint32_t elapsed = millis() - roundStartMs;
  if(elapsed > roundMs) elapsed = roundMs;
  uint32_t remaining = roundMs - elapsed;

  int fillPx = (int)((uint32_t)(BAR_W-2) * remaining / roundMs);
  if(fillPx == lastBarFill) {
    if(remaining > (roundMs / 10)) return;
  }
  lastBarFill = fillPx;

  uint32_t pct = (uint32_t)remaining * 100 / roundMs;

  uint16_t barColor = C_ACCENT;
  if(pct <= 20) barColor = C_BAD;
  else if(pct <= 50) barColor = C_WARN;

  bool blink = (pct <= 10);
  if(blink) {
    bool on = ((millis() / 180) % 2) == 0;
    if(!on) barColor = TFT_BLACK;
  }

  tft.fillRoundRect(BAR_X+1, BAR_Y+1, BAR_W-2, BAR_H-2, 3, TFT_BLACK);
  if(fillPx > 0 && barColor != TFT_BLACK) {
    tft.fillRoundRect(BAR_X+1, BAR_Y+1, fillPx, BAR_H-2, 3, barColor);
  }
}

// ---------------- SCREENS ----------------
static void drawRfidRetryScreen() {
  ledsOff();
  drawBackground();
  drawTopTitle("Color Match Pairs");
  drawCenterCard("RFID ERROR", "Tap RETRY to reconnect");
  uiButton(BTN_RETRY_X, BTN_RETRY_Y, BTN_RETRY_W, BTN_RETRY_H, C_WARN, "RETRY");
  drawBackButton();
}

static void drawLevelScreen() {
  ledsOff();
  drawBackground();
  drawTopTitle("Color Match Pairs");
  drawCenterCard("Choose Difficulty", "Match all pairs before time ends");
  uiButton(BTN_X, BTN_EASY_Y, BTN_W, BTN_H, C_OK,   "EASY");
  uiButton(BTN_X, BTN_MED_Y,  BTN_W, BTN_H, C_WARN, "MEDIUM");
  uiButton(BTN_X, BTN_HARD_Y, BTN_W, BTN_H, C_BAD,  "HARD");
  drawBackButton();
}

static void drawPlayScreenHeader() {
  drawBackground();
  drawTopTitle("Match the Colors");
  drawCenterCard("SCAN 2 TAGS", "Same color = match (same tag ignored)");
  drawTimeBarFrame();

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, TFT_BLACK);

  tft.fillRect(0, 210, 320, 30, TFT_BLACK);
  tft.drawString(String("Coins: ") + coinsTotal, 60, 224);
  tft.drawString(String("Pairs: ") + pairsMatched + "/" + boardPairs, 250, 224);

  tft.setTextDatum(TL_DATUM);
}

static void drawDoneScreen(bool win, bool timeout=false) {
  ledsOff();
  drawBackground();

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  if(win) {
    tft.setTextColor(C_OK, TFT_BLACK);
    tft.drawString("GREAT!", 160, 40);
  } else {
    tft.setTextColor(C_BAD, TFT_BLACK);
    tft.drawString(timeout ? "TIME!" : "OOPS!", 160, 40);
  }

  int msgY = 70;
  tft.fillRoundRect(30, msgY, 260, 44, 14, C_PANEL2);
  tft.drawRoundRect(30, msgY, 260, 44, 14, TFT_WHITE);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, C_PANEL2);
  if(win) tft.drawString("All pairs matched!", 160, msgY + 22);
  else if(timeout) tft.drawString("Try faster next time", 160, msgY + 22);
  else tft.drawString("Wrong pair", 160, msgY + 22);

  int bx = 30, by = 122, bw = 260, bh = 44;
  tft.fillRoundRect(bx, by, bw, bh, 14, TFT_BLACK);
  tft.drawRoundRect(bx, by, bw, bh, 14, TFT_WHITE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, TFT_BLACK);
  tft.drawString("Earned this round", bx + 12, by + 6);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String("Pairs +") + coinsFromMatches, bx + 12, by + 24);
  tft.drawString(String("Win +")   + coinsFromBonus,   bx + 120, by + 24);

  tft.setTextDatum(TR_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(C_ACCENT, TFT_BLACK);
  tft.drawString(String("+") + coinsRound, bx + bw - 12, by + 30);

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, TFT_BLACK);
  tft.drawString(String("Score: ") + score + "   Total Coins: " + coinsTotal, 160, 178);

  // ✅ DONE buttons (NO BACK here)
  uiButton(END_PLAY_X, END_BTN_Y, END_BTN_W, END_BTN_H, C_ACCENT, "PLAY AGAIN");
  uiButton(END_MENU_X, END_BTN_Y, END_BTN_W, END_BTN_H, C_WARN,   "GAMES MENU");

  tft.setTextDatum(TL_DATUM);
}


// ---------------- LEVEL SETTINGS ----------------
static void applyLevelSettings() {
  if(level == LV_EASY)      { boardPairs = 4; roundMs = 18000; }
  else if(level == LV_MEDIUM){ boardPairs = 6; roundMs = 15000; }
  else                      { boardPairs = 8; roundMs = 13500; }

  if(boardPairs > 8) boardPairs = 8;
  if(boardPairs < 1) boardPairs = 1;
}

// ---------------- BOARD GENERATION ----------------
static void generateBoard() {
  activeCount = boardPairs * 2;
  if(activeCount > MAX_ACTIVE) activeCount = MAX_ACTIVE;

  int n=0;
  while(n < activeCount) {
    uint8_t led = LED_ZONES[random(NUM_ZONES)];
    if(!ledInUse(led, n)) {
      activeLed[n] = led;
      matched[n] = false;
      n++;
    }
  }

  int idx = 0;
  for(int pairId=0; pairId < boardPairs && (idx+1)<activeCount; pairId++){
    uint8_t pid = (uint8_t)pairId;
    if(pid >= PALETTE_LEN) pid = (uint8_t)(PALETTE_LEN - 1);
    colorId[idx++] = pid;
    colorId[idx++] = pid;
  }

  for(int i=activeCount-1;i>0;i--){
    int j = random(0, i+1);
    uint8_t tmp = colorId[i];
    colorId[i] = colorId[j];
    colorId[j] = tmp;
  }

  hasFirst = false;
  firstIdx = -1;
  pairsMatched = 0;
}

static void renderBoardLeds() {
  strip.clear();
  for(int i=0;i<activeCount;i++){
    if(matched[i]) continue;
    uint8_t pid = colorId[i];
    if(pid >= PALETTE_LEN) pid = (uint8_t)(PALETTE_LEN - 1);
    strip.setPixelColor(activeLed[i], PALETTE[pid]);
  }
  strip.show();
}

static void flashMatchedPair(int a, int b) {
  strip.setPixelColor(activeLed[a], strip.Color(0, 220, 0));
  strip.setPixelColor(activeLed[b], strip.Color(0, 220, 0));
  strip.show();
  delay(180);

  strip.setPixelColor(activeLed[a], PALETTE[colorId[a]]);
  strip.setPixelColor(activeLed[b], PALETTE[colorId[b]]);
  strip.show();
  delay(90);
}

static int pointsPerMatch() {
  if(level == LV_EASY) return 5;
  if(level == LV_MEDIUM) return 8;
  return 12;
}

static int coinsPerMatch() {
  if(level == LV_EASY) return 1;
  if(level == LV_MEDIUM) return 2;
  return 3;
}
static int coinsWinBonus() {
  if(level == LV_EASY) return 4;
  if(level == LV_MEDIUM) return 6;
  return 8;
}

// ---------------- FLOW ----------------
static void doCountdown() {
  drawBackground();
  drawTopTitle("Get ready");
  drawCenterCard("STARTING", "Match the color pairs");

  for(int n=3; n>=1; n--){
    int x=70, y=96, w=180, h=84;
    tft.fillRoundRect(x+3,y+3,w,h,16,TFT_BLACK);
    tft.fillRoundRect(x,y,w,h,16,C_PANEL2);
    tft.drawRoundRect(x,y,w,h,16,TFT_WHITE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(8);
    tft.setTextColor(TFT_WHITE, C_PANEL2);
    tft.drawString(String(n), 160, 138);
    tft.setTextDatum(TL_DATUM);

    flashAll(strip.Color(0,120,180), 1, 70, 70);
    delay(650);

    drawBackground();
    drawTopTitle("Get ready");
    drawCenterCard("STARTING", "Match the color pairs");
  }
  state = ST_SHOW_BOARD;
}

static void showBoard() {
  coinsRound = 0;
  coinsFromMatches = 0;
  coinsFromBonus = 0;

  score = 0;
  pairsMatched = 0;

  generateBoard();
  drawPlayScreenHeader();
  renderBoardLeds();

  roundStartMs = millis();
  inputPhase = WAIT_FOR_TAG;
  kickPN532();
  lastRFIDOkMs = millis();

  state = ST_PLAY;
}

// ---------------- INPUT ----------------
static void handlePlay() {
  if(millis() - lastRFIDOkMs > RFID_STUCK_MS) recoverRFID();

  if(millis() - roundStartMs > roundMs) {
    fillAll(strip.Color(180,0,0));
    delay(450);
    ledsOff();
    state = ST_DONE;
    drawDoneScreen(false, true);
    return;
  }

  uint8_t uid4[4];
  bool tagPresent = readUID4(uid4);

  if(inputPhase == WAIT_FOR_TAG) {
    if(!tagPresent) return;

    int idx = findActiveIndexByUID(uid4);
    if(idx < 0) {
      flashAll(strip.Color(180,0,0), 1, 60, 60);
      inputPhase = WAIT_FOR_LIFT;
      return;
    }
    if(matched[idx]) {
      blinkLed(activeLed[idx], strip.Color(255, 200, 40), 1, 90, 60);
      inputPhase = WAIT_FOR_LIFT;
      return;
    }

    if(!hasFirst) {
      hasFirst = true;
      firstIdx = idx;
      blinkLed(activeLed[idx], strip.Color(255,255,255), 1, 90, 60);
      renderBoardLeds();
      inputPhase = WAIT_FOR_LIFT;
      return;
    }

    if(idx == firstIdx) {
      blinkLed(activeLed[idx], strip.Color(255, 200, 40), 1, 110, 70);
      renderBoardLeds();
      inputPhase = WAIT_FOR_LIFT;
      return;
    }

    bool ok = (colorId[idx] == colorId[firstIdx]);

    if(ok) {
      flashMatchedPair(firstIdx, idx);
      matched[firstIdx] = true;
      matched[idx] = true;
      pairsMatched++;
      score += pointsPerMatch();

      int c = coinsPerMatch();
      coinsTotal += c;
      coinsRound += c;
      coinsFromMatches += c;

      drawPlayScreenHeader();
      renderBoardLeds();

      hasFirst = false;
      firstIdx = -1;

      if(pairsMatched >= boardPairs) {
        flashAll(strip.Color(0,180,0), 2, 130, 90);

        int b = coinsWinBonus();
        coinsTotal += b;
        coinsRound += b;
        coinsFromBonus += b;

        state = ST_DONE;
        drawDoneScreen(true, false);
        return;
      }

    } else {
      fillAll(strip.Color(180,0,0));
      delay(450);
      ledsOff();
      state = ST_DONE;
      drawDoneScreen(false, false);
      return;
    }

    inputPhase = WAIT_FOR_LIFT;
    return;
  }

  if(tagPresent) return;
  inputPhase = WAIT_FOR_TAG;
  kickPN532();
}

// ============================================================
//  PUBLIC API
// ============================================================
void Game3_begin() {
  randomSeed(micros());

  uint32_t v = nfc.getFirmwareVersion();
  if(!v) {
    state = ST_RFID_RETRY;
    drawRfidRetryScreen();
    return;
  }

  state = ST_PICK_LEVEL;
  level = LV_NONE;
  drawLevelScreen();
  lastRFIDOkMs = millis();
}

void Game3_update() {
  int sx, sy;

  // Global BACK (enabled on retry/level/play screens, disabled on DONE)
  if(state != ST_DONE && Touch_pressed(sx, sy)) {
    if(hitRectMapped(sx, sy, BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H)) {
      waitTouchRelease();
      ledsOff();
      g_screen = SCR_MENU;
      Menu_draw();
      return;
    }
  }

  // RFID RETRY
  if(state == ST_RFID_RETRY) {
    if(Touch_pressed(sx, sy)) {
      if(hitRectMapped(sx, sy, BTN_RETRY_X, BTN_RETRY_Y, BTN_RETRY_W, BTN_RETRY_H)) {
        waitTouchRelease();

        drawCenterCard("RETRYING...", "Please wait");
        nfc.begin();
        delay(30);
        uint32_t v = nfc.getFirmwareVersion();
        if(v) {
          nfc.SAMConfig();
          state = ST_PICK_LEVEL;
          drawLevelScreen();
          lastRFIDOkMs = millis();
        } else {
          drawRfidRetryScreen();
        }
      }
    }
    delay(20);
    return;
  }

  // PICK LEVEL
  if(state == ST_PICK_LEVEL) {
    if(Touch_pressed(sx, sy)) {
      bool inEasy = hitRectMapped(sx, sy, BTN_X, BTN_EASY_Y, BTN_W, BTN_H);
      bool inMed  = hitRectMapped(sx, sy, BTN_X, BTN_MED_Y,  BTN_W, BTN_H);
      bool inHard = hitRectMapped(sx, sy, BTN_X, BTN_HARD_Y, BTN_W, BTN_H);

      if(!inEasy && !inMed && !inHard) { delay(10); return; }

      if(inEasy)      level = LV_EASY;
      else if(inMed)  level = LV_MEDIUM;
      else            level = LV_HARD;

      waitTouchRelease();

      applyLevelSettings();
      state = ST_COUNTDOWN;
      doCountdown();
    }
    delay(10);
    return;
  }

  if(state == ST_SHOW_BOARD) { showBoard(); delay(10); return; }

  if(state == ST_PLAY) {
    updateTimeBar();
    handlePlay();
    delay(5);
    return;
  }

  // DONE: NO BACK. Two buttons only.
  if(state == ST_DONE) {
    if(Touch_pressed(sx, sy)) {
      bool playHit = hitRectMapped(sx, sy, END_PLAY_X, END_BTN_Y, END_BTN_W, END_BTN_H);
      bool menuHit = hitRectMapped(sx, sy, END_MENU_X, END_BTN_Y, END_BTN_W, END_BTN_H);

      waitTouchRelease();

      if(playHit) {
        level = LV_NONE;
        state = ST_PICK_LEVEL;
        drawLevelScreen();   // ✅ Play Again -> Game3 levels
        return;
      }

      if(menuHit) {
        ledsOff();
        g_screen = SCR_MENU; // ✅ Games Menu
        Menu_draw();
        return;
      }
    }
    delay(20);
    return;
  }
}

