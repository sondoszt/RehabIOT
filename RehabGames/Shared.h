#pragma once
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>

// ---------------- PINS (your wiring) ----------------
#define TFT_CS_PIN   15
#define TOUCH_CS     5
#define TOUCH_IRQ    255

#define PN532_SDA    32
#define PN532_SCL    33

#define LED_PIN      25
#define LED_COUNT    32

#define SPI_SCK_PIN  18
#define SPI_MISO_PIN 19
#define SPI_MOSI_PIN 23

// ---------------- SCREEN ----------------
static const int SCREEN_W = 320;
static const int SCREEN_H = 240;
static const uint8_t TFT_ROT = 1;
static const uint8_t TS_ROT  = 1;

// touch filter
static const int Z_MIN = 200;
static const int Z_MAX = 3800;

// -------------- Touch calibration (YOUR working values) --------------
extern int  TOUCH_X_MIN;
extern int  TOUCH_X_MAX;
extern int  TOUCH_Y_MIN;
extern int  TOUCH_Y_MAX;

extern bool TOUCH_SWAP_XY;
extern bool TOUCH_INVERT_X;
extern bool TOUCH_INVERT_Y;

// ---------------- GLOBAL OBJECTS (single instance!) ----------------
extern TFT_eSPI tft;
extern XPT2046_Touchscreen ts;
extern Adafruit_PN532 nfc;
extern Adafruit_NeoPixel strip;

// ---------------- APP SCREEN ----------------
enum AppScreen { SCR_MENU, SCR_GAME1, SCR_GAME2, SCR_GAME3 };
extern AppScreen g_screen;

// ---------------- Touch API ----------------
bool Touch_pressed(int &sx, int &sy);                 // debounced press -> screen coords
bool Touch_pressedRaw(int &sx, int &sy, TS_Point &raw);

bool inRect(int x,int y,int rx,int ry,int rw,int rh);

// init all hardware once (called from setup in .ino)
void Shared_setupHardware();

// ---------------- Game entry points ----------------
void Shared_touchTick();
void drawMenu();

void Game1_begin();
void Game1_update();

void Game2_begin();
void Game2_update();

void Game3_begin();
void Game3_update();
