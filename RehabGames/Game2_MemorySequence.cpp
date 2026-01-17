#include "Shared.h"

// must exist in your menu file
void Menu_draw();

// ===================== COLORS =====================
static const uint16_t C_BG     = 0x08A3;
static const uint16_t C_PANEL2 = 0x18E7;
static const uint16_t C_MUTED  = 0xBDF7;
static const uint16_t C_ACCENT = 0x07FF;
static const uint16_t C_OK     = 0x07E0;
static const uint16_t C_BAD    = 0xF800;
static const uint16_t C_WARN   = 0xFD20;

uint32_t SEQ_COLORS[3];

// ===================== LED ZONES + UID MAP =====================
static const uint8_t LED_ZONES[] = {0,2,4,6,9,11,13,15,16,18,20,22,25,27,29,31};
static const int NUM_ZONES = sizeof(LED_ZONES) / sizeof(LED_ZONES[0]);

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

// ===================== GAME STATE =====================
enum Level { LV_NONE, LV_EASY, LV_MEDIUM, LV_HARD };
enum State { ST_RFID_RETRY, ST_PICK_LEVEL, ST_COUNTDOWN, ST_SHOW_SEQ, ST_INPUT_SEQ, ST_DONE };
enum InputPhase { WAIT_FOR_TAG, WAIT_FOR_LIFT };

static Level level = LV_NONE;
static State state = ST_PICK_LEVEL;
static InputPhase inputPhase = WAIT_FOR_TAG;

static int score = 0;
static int coins = 0;

static const int MAX_SEQ = 10;
static uint8_t sequence[MAX_SEQ];

static int seqBaseLen = 4;
static int seqLen     = 4;
static int userIndex  = 0;

static uint16_t showOnMs  = 520;
static uint16_t showGapMs = 240;

static int noTagStreak = 0;
static const int LIFT_STREAK_REQUIRED = 4;

static const uint32_t INPUT_TIMEOUT_MS_EASY   = 12000;
static const uint32_t INPUT_TIMEOUT_MS_MEDIUM = 10000;
static const uint32_t INPUT_TIMEOUT_MS_HARD   = 8000;
static uint32_t lastUserActionMs = 0;

static const int BAR_X = 40;
static const int BAR_Y = 30;
static const int BAR_W = 240;
static const int BAR_H = 10;
static uint32_t lastBarDrawMs = 0;
static int lastBarFill = -1;

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

static const int BTN_RETRY_X = 80;
static const int BTN_RETRY_Y = 168;
static const int BTN_RETRY_W = 160;
static const int BTN_RETRY_H = 46;

// Back button (top-left)
static const int BTN_BACK_X = 10;
static const int BTN_BACK_Y = 6;
static const int BTN_BACK_W = 70;
static const int BTN_BACK_H = 24;

// DONE buttons (Game1 style)
static const int END_BTN_Y = 186;
static const int END_BTN_H = 40;
static const int END_BTN_W = 132;
static const int END_GAP   = 16;
static const int END_PLAY_X = 20;
static const int END_MENU_X = END_PLAY_X + END_BTN_W + END_GAP;

// ===================== TOUCH HELPERS (robust if X mirrored) =====================

static void waitTouchRelease() {
  int tx, ty;
  while (Touch_pressed(tx, ty)) delay(5);
}

// ===================== LED HELPERS =====================
static void ledsOff(){ strip.clear(); strip.show(); }
static void lightOne(uint8_t idx, uint32_t c){ strip.clear(); strip.setPixelColor(idx,c); strip.show(); }
static void fillAll(uint32_t c){ for(int i=0;i<LED_COUNT;i++) strip.setPixelColor(i,c); strip.show(); }
static void flashAll(uint32_t c, int times, int onMs=120, int offMs=80){
  for(int t=0;t<times;t++){ fillAll(c); delay(onMs); ledsOff(); delay(offMs); }
}

// ===================== RFID HELPERS =====================
static bool readUID4(uint8_t out[4]) {
  uint8_t uid[7], len;
  if(!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 40)) return false;
  if(len < 4) return false;
  for(int i=0;i<4;i++) out[i]=uid[i];
  return true;
}
static const uint8_t* expectedUIDForLed(uint8_t led){
  for(int i=0;i<MAP_LEN;i++) if(MAP[i].led == led) return MAP[i].uid;
  return nullptr;
}
static bool uidEq(const uint8_t*a,const uint8_t*b){ for(int i=0;i<4;i++) if(a[i]!=b[i]) return false; return true; }

static bool initPN532Once() {
  nfc.begin();
  delay(30);
  uint32_t v = nfc.getFirmwareVersion();
  if(!v) return false;
  nfc.SAMConfig();
  return true;
}
static bool initPN532WithFallback() {
  Wire.setClock(100000);
  Wire.setTimeOut(50);
  return initPN532Once();
}
static void kickPN532(){ nfc.SAMConfig(); }

// ===================== UI HELPERS =====================
static uint16_t blend565(uint16_t c1, uint16_t c2, uint8_t t) {
  uint8_t r1 = (c1>>11)&0x1F, g1=(c1>>5)&0x3F, b1=c1&0x1F;
  uint8_t r2 = (c2>>11)&0x1F, g2=(c2>>5)&0x3F, b2=c2&0x1F;
  uint8_t r=(r1*(255-t)+r2*t)/255, g=(g1*(255-t)+g2*t)/255, b=(b1*(255-t)+b2*t)/255;
  return (r<<11)|(g<<5)|b;
}
static void fillGradV(int x,int y,int w,int h,uint16_t top,uint16_t bot,int step=2){
  for(int i=0;i<h;i+=step){ uint8_t t=(uint32_t)i*255/(h-1); uint16_t c=blend565(top,bot,t); tft.fillRect(x,y+i,w,step,c); }
}
static void drawBackground(){ fillGradV(0,0,SCREEN_W,SCREEN_H,C_BG,0x0008,2); for(int i=0;i<18;i++) tft.drawPixel(random(0,SCREEN_W), random(0,SCREEN_H), TFT_WHITE); }

static void drawBackButton(){
  tft.fillRoundRect(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, 8, C_PANEL2);
  tft.drawRoundRect(BTN_BACK_X, BTN_BACK_Y, BTN_BACK_W, BTN_BACK_H, 8, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, C_PANEL2);
  tft.drawString("< Back", BTN_BACK_X + BTN_BACK_W/2, BTN_BACK_Y + BTN_BACK_H/2);
  tft.setTextDatum(TL_DATUM);
}
#define TOUCH_MIRROR_X 1

static inline int mapX(int sx){
  return TOUCH_MIRROR_X ? (SCREEN_W - 1 - sx) : sx;
}

static bool hitRectMapped(int sx,int sy,int rx,int ry,int rw,int rh){
  int x = mapX(sx);
  return inRect(x, sy, rx, ry, rw, rh);
}
static bool backTapped(int sx,int sy){
  return hitRectMapped(sx,sy, BTN_BACK_X,BTN_BACK_Y,BTN_BACK_W,BTN_BACK_H);
}

// ---- TOUCH ORIENTATION (Game2) ----
// If your touch is mirrored in X, keep this = 1



static void goMenu(){
  ledsOff();
  g_screen = SCR_MENU;
  Menu_draw();
}





static void drawTopTitle(const char* title){
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, TFT_BLACK);
  tft.drawString(title, 160, 18);
  tft.setTextDatum(TL_DATUM);
}

static void uiButton(int x,int y,int w,int h,uint16_t bg,const char* label){
  tft.fillRoundRect(x+3,y+3,w,h,16,TFT_BLACK);
  tft.fillRoundRect(x,y,w,h,16,bg);
  tft.drawRoundRect(x,y,w,h,16,TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  // shrink font for narrow buttons (like Game1)
  if(w <= 140) tft.setTextFont(2);
  else         tft.setTextFont(4);
  tft.setTextColor(TFT_BLACK, bg);
  tft.drawString(label, x+w/2+1, y+h/2+1);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawString(label, x+w/2, y+h/2);
  tft.setTextDatum(TL_DATUM);
}

static void drawCenterCard(const char* top, const char* bottom){
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

// ===================== TIMEOUT BAR =====================
static uint32_t inputTimeoutMs(){
  if(level == LV_EASY) return INPUT_TIMEOUT_MS_EASY;
  if(level == LV_MEDIUM) return INPUT_TIMEOUT_MS_MEDIUM;
  return INPUT_TIMEOUT_MS_HARD;
}
static void resetInputTimer(){ lastUserActionMs = millis(); }

static void drawTimeoutBarFrame(){
  tft.drawRoundRect(BAR_X, BAR_Y, BAR_W, BAR_H, 4, TFT_WHITE);
  tft.fillRoundRect(BAR_X+1, BAR_Y+1, BAR_W-2, BAR_H-2, 3, TFT_BLACK);
  lastBarFill = -1;
  lastBarDrawMs = 0;
}
static void updateTimeoutBar(){
  if(state != ST_INPUT_SEQ) return;
  if(millis() - lastBarDrawMs < 120) return;
  lastBarDrawMs = millis();

  uint32_t tmo = inputTimeoutMs();
  uint32_t elapsed = millis() - lastUserActionMs;
  if(elapsed > tmo) elapsed = tmo;

  int fillPx = (int)((uint32_t)(BAR_W-2) * (tmo - elapsed) / tmo);
  if(fillPx == lastBarFill) return;
  lastBarFill = fillPx;

  tft.fillRoundRect(BAR_X+1, BAR_Y+1, BAR_W-2, BAR_H-2, 3, TFT_BLACK);
  if(fillPx > 0) tft.fillRoundRect(BAR_X+1, BAR_Y+1, fillPx, BAR_H-2, 3, C_ACCENT);
}

// ===================== SCREENS =====================
static void drawRfidRetryScreen(){
  ledsOff(); drawBackground();
  drawTopTitle("Memory Sequence");
  drawCenterCard("RFID ERROR", "Tap RETRY to reconnect");
  uiButton(BTN_RETRY_X, BTN_RETRY_Y, BTN_RETRY_W, BTN_RETRY_H, C_WARN, "RETRY");
  drawBackButton();
}

static void drawLevelScreen(){
  ledsOff(); drawBackground();
  drawTopTitle("Memory Sequence");
  drawCenterCard("Choose Difficulty", "Tap to start");
  uiButton(BTN_X, BTN_EASY_Y, BTN_W, BTN_H, C_OK,   "EASY");
  uiButton(BTN_X, BTN_MED_Y,  BTN_W, BTN_H, C_WARN, "MEDIUM");
  uiButton(BTN_X, BTN_HARD_Y, BTN_W, BTN_H, C_BAD,  "HARD");
  drawBackButton();
}

static void drawRepeatScreen(){
  drawBackground();
  drawTopTitle("Your turn");
  drawCenterCard("REPEAT", "Scan tags in the same order");
  drawBackButton();
}

static void drawDoneScreen(bool win, bool timedOut=false){
  ledsOff(); drawBackground();

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED, TFT_BLACK);
  tft.drawString("Session finished", 160, 18);

  int cardX = 24, cardY = 44, cardW = 272, cardH = 78;
  tft.fillRoundRect(cardX+3, cardY+3, cardW, cardH, 16, TFT_BLACK);
  tft.fillRoundRect(cardX,   cardY,   cardW, cardH, 16, C_PANEL2);
  tft.drawRoundRect(cardX,   cardY,   cardW, cardH, 16, TFT_WHITE);

  tft.setTextFont(4);
  tft.setTextColor(C_ACCENT, C_PANEL2);
  tft.drawString(win ? "NICE!" : (timedOut ? "TIME!" : "OOPS!"), 160, cardY + 18);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, C_PANEL2);
  if(win) tft.drawString("Sequence completed!", 160, cardY + 52);
  else if(timedOut) tft.drawString("No scan in time", 160, cardY + 52);
  else tft.drawString("Wrong tag scanned", 160, cardY + 52);

  // score + coins pill (like Game1 style)
  int pillX = 52, pillY = cardY + cardH + 10, pillW = 216, pillH = 26;
  tft.fillRoundRect(pillX, pillY, pillW, pillH, 12, 0x0841);
  tft.drawRoundRect(pillX, pillY, pillW, pillH, 12, TFT_WHITE);

  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, 0x0841);
  tft.drawString(String("Score ") + score + "   + " + coins + " Coins", 160, pillY + pillH/2);

  // Buttons
  uiButton(END_PLAY_X, END_BTN_Y, END_BTN_W, END_BTN_H, C_ACCENT, "PLAY AGAIN");
  uiButton(END_MENU_X, END_BTN_Y, END_BTN_W, END_BTN_H, C_WARN,   "GAMES MENU");

  
  tft.setTextDatum(TL_DATUM);
}

// ===================== GAME LOGIC =====================
static uint8_t pickZoneNotSame(uint8_t prev){
  uint8_t led;
  do { led = LED_ZONES[random(NUM_ZONES)]; } while(led == prev && NUM_ZONES > 1);
  return led;
}
static void generateSequence(int len){
  uint8_t prev = 255;
  for(int i=0;i<len;i++){ sequence[i] = pickZoneNotSame(prev); prev = sequence[i]; }
}
static void applyLevelSettings(){
  if(level == LV_EASY){ seqBaseLen=3; showOnMs=540; showGapMs=250; }
  else if(level == LV_MEDIUM){ seqBaseLen=4; showOnMs=430; showGapMs=210; }
  else { seqBaseLen=5; showOnMs=330; showGapMs=170; }
}
static void startNewRound(){
  seqLen = seqBaseLen;
  userIndex = 0;
  inputPhase = WAIT_FOR_TAG;
  noTagStreak = 0;
  generateSequence(seqLen);
  resetInputTimer();
}

static void doCountdown(){
  drawBackground(); drawTopTitle("Get ready");
  drawCenterCard("STARTING", "Watch closely...");
  drawBackButton();

  for(int n=3;n>=1;n--){
    int x=70,y=96,w=180,h=84;
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

    drawBackground(); drawTopTitle("Get ready");
    drawCenterCard("STARTING", "Watch closely...");
    drawBackButton();
  }
  state = ST_SHOW_SEQ;
}

static void showSequence(){
  drawBackground(); drawTopTitle("Watch the sequence");
  drawCenterCard("WATCH", "Then repeat with RFID");
  drawBackButton();
  delay(300);

  for(int i=0;i<seqLen;i++){
    uint8_t led = sequence[i];
    uint32_t c  = SEQ_COLORS[i % 3];
    lightOne(led, c); delay(showOnMs);
    ledsOff(); delay(showGapMs);
  }

  userIndex=0; inputPhase=WAIT_FOR_TAG; noTagStreak=0;
  drawRepeatScreen();
  drawTimeoutBarFrame();
  state = ST_INPUT_SEQ;
  kickPN532();
  resetInputTimer();
}

static int pointsPerStep(){ if(level==LV_EASY) return 1; if(level==LV_MEDIUM) return 2; return 3; }
static int winBonus(){ if(level==LV_EASY) return 5; if(level==LV_MEDIUM) return 8; return 12; }
static int coinsReward(){ if(level==LV_EASY) return 1; if(level==LV_MEDIUM) return 2; return 3; }

static void feedbackForStep(bool ok, uint8_t led){
  if(ok){ lightOne(led, strip.Color(0,180,0)); delay(160); ledsOff(); }
  else { fillAll(strip.Color(180,0,0)); delay(450); ledsOff(); }
}

static void handleInputSequence(){
  if(millis() - lastUserActionMs > inputTimeoutMs()){
    fillAll(strip.Color(180,0,0)); delay(450); ledsOff();
    state = ST_DONE; drawDoneScreen(false, true); return;
  }

  uint8_t uid4[4];
  bool tagPresent = readUID4(uid4);

  if(inputPhase == WAIT_FOR_TAG){
    if(!tagPresent) return;
    resetInputTimer();

    uint8_t expectedLed = sequence[userIndex];
    const uint8_t* expUid = expectedUIDForLed(expectedLed);
    bool correct = (expUid && uidEq(uid4, expUid));

    feedbackForStep(correct, expectedLed);

    inputPhase = WAIT_FOR_LIFT;
    noTagStreak = 0;

    if(!correct){ state = ST_DONE; drawDoneScreen(false,false); return; }

    score += pointsPerStep();
    userIndex++;

    if(userIndex >= seqLen){
      fillAll(strip.Color(0,180,0)); delay(450); ledsOff();
      score += winBonus();
      coins += coinsReward();
      state = ST_DONE; drawDoneScreen(true,false); return;
    }
    return;
  }

  if(tagPresent){ noTagStreak=0; return; }
  noTagStreak++;
  if(noTagStreak >= LIFT_STREAK_REQUIRED){
    inputPhase = WAIT_FOR_TAG;
    noTagStreak = 0;
    kickPN532();
  }
}

// ===================== PUBLIC API =====================
void Game2_begin(){
  SEQ_COLORS[0] = strip.Color(160, 0, 255);
  SEQ_COLORS[1] = strip.Color(0, 120, 255);
  SEQ_COLORS[2] = strip.Color(220, 170, 0);

  if(initPN532WithFallback()){
    state = ST_PICK_LEVEL;
    drawLevelScreen();
  } else {
    state = ST_RFID_RETRY;
    drawRfidRetryScreen();
  }
}

void Game2_update(){
  int sx, sy;

  // BACK for all screens
 // BACK button — disabled on DONE screen
if(state != ST_DONE && Touch_pressed(sx, sy)){
  if(backTapped(sx, sy)){
    waitTouchRelease();
    goMenu();
    return;
  }
}


  if(state == ST_RFID_RETRY){
    if(Touch_pressed(sx, sy)){
      if(hitRectMapped(sx,sy, BTN_RETRY_X,BTN_RETRY_Y,BTN_RETRY_W,BTN_RETRY_H)){
        waitTouchRelease();
        drawCenterCard("RETRYING...", "Please wait");
        if(initPN532WithFallback()){ state = ST_PICK_LEVEL; drawLevelScreen(); }
        else drawRfidRetryScreen();
      }
    }
    delay(20); return;
  }

  if(state == ST_PICK_LEVEL){
    if(Touch_pressed(sx, sy)){
      if(hitRectMapped(sx,sy, BTN_X,BTN_EASY_Y,BTN_W,BTN_H)) level = LV_EASY;
      else if(hitRectMapped(sx,sy, BTN_X,BTN_MED_Y,BTN_W,BTN_H)) level = LV_MEDIUM;
      else if(hitRectMapped(sx,sy, BTN_X,BTN_HARD_Y,BTN_W,BTN_H)) level = LV_HARD;
      else { delay(10); return; }

      waitTouchRelease();
      applyLevelSettings();
      score = 0;
      coins = 0; // if you want TOTAL across sessions, remove this line
      startNewRound();
      state = ST_COUNTDOWN;
      doCountdown();
    }
    delay(10); return;
  }

  if(state == ST_SHOW_SEQ){ showSequence(); delay(10); return; }

  if(state == ST_INPUT_SEQ){
    updateTimeoutBar();
    handleInputSequence();
    delay(5); return;
  }

 if(state == ST_DONE){
  if(Touch_pressed(sx, sy)){

    // PLAY AGAIN (left)
    if(hitRectMapped(sx, sy, END_PLAY_X, END_BTN_Y, END_BTN_W, END_BTN_H)){
      waitTouchRelease();
      level = LV_NONE;
      state = ST_PICK_LEVEL;
      drawLevelScreen();
      return;
    }

    // GAMES MENU (right)
    if(hitRectMapped(sx, sy, END_MENU_X, END_BTN_Y, END_BTN_W, END_BTN_H)){
      waitTouchRelease();
      goMenu();
      return;
    }

    // touched somewhere else
    waitTouchRelease();
  }
  delay(20);
  return;
}




  delay(10);
}
