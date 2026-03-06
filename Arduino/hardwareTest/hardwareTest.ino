/*
 * hardwareTest.ino
 * ================
 * Initial hardware test for PrimusV2 prototype.
 *
 * Hardware:
 *   - Adafruit ESP32-S3 Reverse TFT Feather
 *   - Adafruit NeoPXL8 Friend breakout (3.3V → 5V level shifter)
 *   - 3x WS2812B NeoPixel strips (different lengths)
 *
 * Wiring (ESP32-S3 → NeoPXL8 Friend → NeoPixels):
 *   Strip 0: GPIO18 (A0) → PXL8 port 0 → 32 LEDs
 *   Strip 1: GPIO17 (A1) → PXL8 port 1 → 68 LEDs
 *   Strip 2: GPIO16 (A2) → PXL8 port 2 → 72 LEDs
 *
 * Controls:
 *   D0 button → cycle test mode on Strip 0 (A0)
 *   D1 button → cycle test mode on Strip 1 (A1)
 *   D2 button → cycle test mode on Strip 2 (A2)
 *
 * Libraries required (install via Arduino Library Manager):
 *   - Adafruit_NeoPXL8
 *   - Adafruit_NeoPixel  (dependency, auto-installed)
 *   - Adafruit_ST7789     (for built-in TFT)
 *   - Adafruit_GFX        (dependency)
 *
 * Board selection in Arduino IDE:
 *   "Adafruit Feather ESP32-S3 Reverse TFT"
 */

#include <Adafruit_NeoPXL8.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// ===========================================================================
//  Configuration
// ===========================================================================

#define NUM_STRIPS    3     // Active strips (out of 8 possible)
#define MAX_LEDS      72    // Longest strip — NeoPXL8 allocates this for all 8
#define BRIGHTNESS    50    // 0-255, kept low for safe USB-powered bench testing

// Actual LED count per strip
const uint16_t stripLeds[NUM_STRIPS] = { 32, 68, 72 };

// Strip label names for display
const char* stripNames[NUM_STRIPS] = { "A0", "A1", "A2" };

// Strip display colors (for TFT)
const uint16_t stripColors[NUM_STRIPS] = { ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE };

// Pin array for NeoPXL8: 8 entries, -1 = unused port
int8_t pins[8] = {
  18,   // Port 0 → Strip 0 (A0 / GPIO18) — 32 LEDs
  17,   // Port 1 → Strip 1 (A1 / GPIO17) — 68 LEDs
  16,   // Port 2 → Strip 2 (A2 / GPIO16) — 72 LEDs
  -1,   // Port 3 — unused
  -1,   // Port 4 — unused
  -1,   // Port 5 — unused
  -1,   // Port 6 — unused
  -1    // Port 7 — unused
};

// NeoPXL8 object: MAX_LEDS per strand, total = MAX_LEDS * 8
Adafruit_NeoPXL8 leds(MAX_LEDS, pins, NEO_GRB + NEO_KHZ800);

// ===========================================================================
//  Buttons — D0 is active-LOW, D1/D2 are active-HIGH
// ===========================================================================

#define BTN_D0  0
#define BTN_D1  1
#define BTN_D2  2

const uint8_t btnPins[NUM_STRIPS] = { BTN_D0, BTN_D1, BTN_D2 };
bool lastBtnState[NUM_STRIPS] = { false, false, false };

// ===========================================================================
//  Test modes per strip
// ===========================================================================

// 0 = Off, 1 = Color Wipe, 2 = All White, 3 = Rainbow, 4 = Pixel March
#define NUM_MODES 5
const char* modeNames[NUM_MODES] = { "Off", "Color Wipe", "White", "Rainbow", "March" };
uint8_t stripMode[NUM_STRIPS] = { 0, 0, 0 };

// Rainbow animation state per strip
long rainbowHue[NUM_STRIPS] = { 0, 0, 0 };

// Pixel march state per strip
uint16_t marchPos[NUM_STRIPS] = { 0, 0, 0 };

// Color wipe state per strip
uint16_t wipePos[NUM_STRIPS] = { 0, 0, 0 };
bool wipeDone[NUM_STRIPS] = { false, false, false };

// ===========================================================================
//  TFT Setup (built-in 240x135 ST7789 on the Reverse TFT Feather)
// ===========================================================================

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ===========================================================================
//  Helper: address a pixel on a specific strip
//  NeoPXL8 flat array: strip r, pixel p → index (r * MAX_LEDS + p)
// ===========================================================================

void setStripPixel(uint8_t strip, uint16_t pixel, uint32_t color) {
  leds.setPixelColor(strip * MAX_LEDS + pixel, color);
}

// Clear only one strip's pixels
void clearStrip(uint8_t strip) {
  for (uint16_t p = 0; p < stripLeds[strip]; p++) {
    setStripPixel(strip, p, 0);
  }
}

// ===========================================================================
//  TFT display helpers
// ===========================================================================

void tftInit() {
  pinMode(TFT_BACKLITE, OUTPUT);
  digitalWrite(TFT_BACKLITE, HIGH);

  #ifdef TFT_I2C_POWER
    pinMode(TFT_I2C_POWER, OUTPUT);
    digitalWrite(TFT_I2C_POWER, HIGH);
    delay(10);
  #endif

  tft.init(135, 240);
  tft.setRotation(3);  // Landscape, USB on the left
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
}

void tftStatus(const char* msg, uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.setTextSize(2);
  tft.setTextColor(color);
  tft.println("NeoPXL8 Test");
  tft.println();
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.println(msg);
}

// Draw the full status screen showing all strip modes
void tftDrawStatus() {
  tft.fillScreen(ST77XX_BLACK);

  // Header
  tft.setCursor(4, 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("PrimusV2 Hardware Test");

  // Divider line
  tft.drawFastHLine(0, 16, 240, ST77XX_WHITE);

  // Strip info — 3 rows
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    int16_t y = 22 + s * 38;

    // Strip name + LED count
    tft.setCursor(4, y);
    tft.setTextSize(2);
    tft.setTextColor(stripColors[s]);
    tft.print(stripNames[s]);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.print(" ");
    tft.print(stripLeds[s]);
    tft.print("px");

    // Button label
    tft.setCursor(110, y);
    tft.setTextColor(ST77XX_YELLOW);
    tft.print("D");
    tft.print(s == 0 ? 0 : s);

    // Current mode (on second line of each row)
    tft.setCursor(4, y + 16);
    tft.setTextSize(2);
    tft.setTextColor(stripMode[s] == 0 ? ST77XX_RED : ST77XX_GREEN);
    tft.print("> ");
    tft.print(modeNames[stripMode[s]]);

    // Clear rest of line
    int16_t cx = tft.getCursorX();
    if (cx < 240) {
      tft.fillRect(cx, y + 16, 240 - cx, 16, ST77XX_BLACK);
    }
  }
}

// Update just one strip's mode line on the TFT (avoids full redraw flicker)
void tftUpdateStripMode(uint8_t s) {
  int16_t y = 22 + s * 38 + 16;

  // Clear the mode line
  tft.fillRect(0, y, 240, 16, ST77XX_BLACK);

  tft.setCursor(4, y);
  tft.setTextSize(2);
  tft.setTextColor(stripMode[s] == 0 ? ST77XX_RED : ST77XX_GREEN);
  tft.print("> ");
  tft.print(modeNames[stripMode[s]]);
}

// ===========================================================================
//  Button reading with debounce
// ===========================================================================

bool readButton(uint8_t index) {
  bool raw = digitalRead(btnPins[index]);
  // D0 is active-LOW (pressed = LOW), D1/D2 are active-HIGH (pressed = HIGH)
  if (index == 0) {
    return !raw;   // Invert for D0
  }
  return raw;
}

void checkButtons() {
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    bool pressed = readButton(s);

    // Detect rising edge (just pressed)
    if (pressed && !lastBtnState[s]) {
      // Cycle to next mode
      stripMode[s] = (stripMode[s] + 1) % NUM_MODES;

      // Reset animation state for this strip
      rainbowHue[s] = 0;
      marchPos[s] = 0;
      wipePos[s] = 0;
      wipeDone[s] = false;

      // Clear strip when switching modes
      clearStrip(s);
      leds.show();

      // Update TFT
      tftUpdateStripMode(s);

      Serial.print("Strip ");
      Serial.print(stripNames[s]);
      Serial.print(" → ");
      Serial.println(modeNames[stripMode[s]]);
    }
    lastBtnState[s] = pressed;
  }
}

// ===========================================================================
//  Per-strip animation functions (non-blocking, called each loop iteration)
// ===========================================================================

// Strip-specific colors for wipe/march
uint32_t stripTestColor(uint8_t s) {
  switch (s) {
    case 0: return leds.Color(255, 0, 0);     // Red
    case 1: return leds.Color(0, 255, 0);     // Green
    case 2: return leds.Color(0, 0, 255);     // Blue
    default: return leds.Color(255, 255, 255);
  }
}

// Mode 1: Color Wipe — fills one pixel per frame, then holds
void animColorWipe(uint8_t s) {
  if (!wipeDone[s]) {
    if (wipePos[s] < stripLeds[s]) {
      setStripPixel(s, wipePos[s], stripTestColor(s));
      wipePos[s]++;
    } else {
      wipeDone[s] = true;
    }
  }
  // Once done, just holds the solid color
}

// Mode 2: All White
void animWhite(uint8_t s) {
  for (uint16_t p = 0; p < stripLeds[s]; p++) {
    setStripPixel(s, p, leds.Color(255, 255, 255));
  }
}

// Mode 3: Rainbow — continuous rotation
void animRainbow(uint8_t s) {
  for (uint16_t p = 0; p < stripLeds[s]; p++) {
    uint16_t hue = rainbowHue[s] + (p * 65536L / stripLeds[s]);
    setStripPixel(s, p, leds.ColorHSV(hue, 255, 255));
  }
  rainbowHue[s] += 512;
  if (rainbowHue[s] >= 65536) rainbowHue[s] -= 65536;
}

// Mode 4: Pixel March — single dot walks down the strip
void animMarch(uint8_t s) {
  // Clear strip
  for (uint16_t p = 0; p < stripLeds[s]; p++) {
    setStripPixel(s, p, 0);
  }
  // Light current pixel
  setStripPixel(s, marchPos[s], stripTestColor(s));
  marchPos[s]++;
  if (marchPos[s] >= stripLeds[s]) marchPos[s] = 0;
}

// Run the current mode for one strip
void runStripMode(uint8_t s) {
  switch (stripMode[s]) {
    case 0:  // Off
      clearStrip(s);
      break;
    case 1:  animColorWipe(s); break;
    case 2:  animWhite(s);     break;
    case 3:  animRainbow(s);   break;
    case 4:  animMarch(s);     break;
  }
}

// ===========================================================================
//  Setup
// ===========================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("=============================");
  Serial.println("PrimusV2 Hardware Test");
  Serial.println("ESP32-S3 + NeoPXL8 Friend");
  Serial.println("=============================");
  Serial.println("Strip config:");
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    Serial.print("  ");
    Serial.print(stripNames[s]);
    Serial.print(": ");
    Serial.print(stripLeds[s]);
    Serial.println(" LEDs");
  }
  Serial.print("Brightness: ");
  Serial.println(BRIGHTNESS);
  Serial.println();

  // --- Buttons ---
  pinMode(BTN_D0, INPUT_PULLUP);   // D0: active-LOW
  pinMode(BTN_D1, INPUT_PULLDOWN); // D1: active-HIGH
  pinMode(BTN_D2, INPUT_PULLDOWN); // D2: active-HIGH

  // --- TFT ---
  tftInit();
  tftStatus("Initializing...", ST77XX_YELLOW);

  // --- NeoPXL8 ---
  if (!leds.begin()) {
    Serial.println("ERROR: NeoPXL8 begin() failed!");
    tftStatus("PXL8 INIT FAIL!", ST77XX_RED);
    while (1) { delay(100); }
  }

  leds.setBrightness(BRIGHTNESS);
  leds.fill(0);
  leds.show();

  Serial.println("NeoPXL8 initialized OK");
  Serial.println("Press D0/D1/D2 to cycle strip modes\n");

  // Draw the main status screen
  tftDrawStatus();
}

// ===========================================================================
//  Main Loop — button-driven per-strip animation
// ===========================================================================

void loop() {
  // Check for button presses
  checkButtons();

  // Run each strip's current animation
  for (uint8_t s = 0; s < NUM_STRIPS; s++) {
    runStripMode(s);
  }

  // Push all pixel data at once
  leds.show();

  // ~30fps frame rate
  delay(33);
}
