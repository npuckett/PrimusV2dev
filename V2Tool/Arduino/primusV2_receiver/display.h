/*
 * display.h — PrimusV2 TFT Display Manager
 * ==========================================
 * Handles the built-in ST7789 240×135 TFT on the ESP32-S3 Reverse TFT Feather.
 * Provides screen modes: startup, connection, running status, error, and test mode.
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include "config.h"

// =====================================================================
//  TFT Object (uses board-defined TFT_CS, TFT_DC, TFT_RST)
// =====================================================================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// =====================================================================
//  Screen Modes
// =====================================================================
enum ScreenMode {
  SCREEN_STARTUP    = 0,
  SCREEN_CONNECTION = 1,
  SCREEN_STATUS     = 2,
  SCREEN_ERROR      = 3,
  SCREEN_TEST       = 4
};

#define NUM_INFO_SCREENS 3  // connection, status, error — cycled by D0
ScreenMode currentScreen = SCREEN_STARTUP;

// Port display colors
const uint16_t portColors[NUM_OUTPUTS] = { ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE };

// =====================================================================
//  Initialization
// =====================================================================
void displayInit() {
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

// =====================================================================
//  Startup Screen
// =====================================================================
void displayStartup() {
  currentScreen = SCREEN_STARTUP;
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(10, 20);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_CYAN);
  tft.println(FIRMWARE_NAME);

  tft.setCursor(10, 55);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Firmware v");
  tft.println(FIRMWARE_VERSION);

  tft.setCursor(10, 75);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("Initializing...");
}

// =====================================================================
//  Connection Screen — WiFi SSID, IP, RSSI
// =====================================================================
void displayConnection(const char* ssid, IPAddress ip, bool connected, int rssi) {
  currentScreen = SCREEN_CONNECTION;
  tft.fillScreen(ST77XX_BLACK);

  // Header
  tft.setCursor(4, 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(FIRMWARE_NAME);
  tft.print(" | Connection");
  tft.drawFastHLine(0, 14, 240, ST77XX_WHITE);

  // WiFi status
  tft.setCursor(4, 20);
  tft.setTextSize(2);
  if (connected) {
    tft.setTextColor(ST77XX_GREEN);
    tft.println("WiFi OK");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("No WiFi");
  }

  // SSID
  tft.setCursor(4, 42);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("SSID: ");
  tft.setTextColor(ST77XX_YELLOW);
  tft.println(ssid);

  // IP Address
  tft.setCursor(4, 56);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("IP:   ");
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  if (connected) {
    tft.print(ip[0]); tft.print(".");
    tft.print(ip[1]); tft.print(".");
    tft.print(ip[2]); tft.print(".");
    tft.println(ip[3]);
  } else {
    tft.println("---.---.---.---");
  }

  // Signal strength
  tft.setCursor(4, 82);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("RSSI: ");
  if (connected) {
    if (rssi > -50) {
      tft.setTextColor(ST77XX_GREEN);
    } else if (rssi > -70) {
      tft.setTextColor(ST77XX_YELLOW);
    } else {
      tft.setTextColor(ST77XX_RED);
    }
    tft.print(rssi);
    tft.println(" dBm");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.println("N/A");
  }

  // Button hint
  tft.setCursor(4, 120);
  tft.setTextSize(1);
  tft.setTextColor(0x7BEF); // dim grey
  tft.print("D0:Screen D1:Test D2:Bright");
}

// =====================================================================
//  Running Status Screen — per-output info, FPS, brightness
// =====================================================================
void displayStatus(OutputConfig outputs[NUM_OUTPUTS], float fps, int brightness,
                   bool outputActive[NUM_OUTPUTS]) {
  currentScreen = SCREEN_STATUS;
  tft.fillScreen(ST77XX_BLACK);

  // Header
  tft.setCursor(4, 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(FIRMWARE_NAME);
  tft.print(" | Status");
  tft.drawFastHLine(0, 14, 240, ST77XX_WHITE);

  // Per-output rows
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    int16_t y = 18 + i * 28;

    // Port indicator
    tft.setCursor(4, y);
    tft.setTextSize(2);
    tft.setTextColor(portColors[i]);
    tft.print(i);

    // Type + pixel count
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(20, y);
    tft.print(outputTypeNames[outputs[i].type]);
    tft.setCursor(20, y + 10);
    tft.print(outputs[i].pixelCount);
    tft.print("px ");
    tft.print(outputs[i].colorOrder == COLOR_RGBW ? "RGBW" : "RGB");

    // Universe
    tft.setCursor(120, y);
    tft.print("U:");
    tft.print(outputs[i].universe);

    // Active indicator
    tft.setCursor(160, y);
    if (outputs[i].type == OUTPUT_OFF) {
      tft.setTextColor(0x7BEF);
      tft.print("OFF");
    } else if (outputActive[i]) {
      tft.setTextColor(ST77XX_GREEN);
      tft.print("RECV");
    } else {
      tft.setTextColor(ST77XX_RED);
      tft.print("IDLE");
    }
  }

  // Footer: FPS + Brightness
  int16_t footerY = 105;
  tft.drawFastHLine(0, footerY, 240, ST77XX_WHITE);
  tft.setCursor(4, footerY + 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("FPS: ");
  tft.setTextColor(ST77XX_CYAN);
  tft.print(fps, 1);

  tft.setCursor(100, footerY + 4);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Bright: ");
  tft.setTextColor(ST77XX_CYAN);
  tft.print(brightness);

  tft.setCursor(180, footerY + 4);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Heap:");
  tft.print(ESP.getFreeHeap() / 1024);
  tft.print("k");
}

// =====================================================================
//  Error Screen
// =====================================================================
void displayError(const char* errorMsg, const char* detail) {
  currentScreen = SCREEN_ERROR;
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(4, 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(FIRMWARE_NAME);
  tft.print(" | Error");
  tft.drawFastHLine(0, 14, 240, ST77XX_RED);

  tft.setCursor(10, 30);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_RED);
  tft.println(errorMsg);

  if (detail != NULL) {
    tft.setCursor(10, 60);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.println(detail);
  }

  tft.setCursor(10, 100);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Check serial for details");
}

// =====================================================================
//  Test Mode Screen
// =====================================================================
void displayTestMode(uint8_t testModeIndex, const char* modeName) {
  currentScreen = SCREEN_TEST;
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(4, 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(FIRMWARE_NAME);
  tft.print(" | Test Mode");
  tft.drawFastHLine(0, 14, 240, ST77XX_MAGENTA);

  tft.setCursor(10, 35);
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_MAGENTA);
  tft.println(modeName);

  tft.setCursor(10, 80);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("ArtNet paused during test");

  tft.setCursor(10, 100);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("D1: next mode  D1 hold: exit");
}

// =====================================================================
//  Quick single-field update: FPS + brightness in status footer
//  (avoids full redraw flicker)
// =====================================================================
void displayUpdateFooter(float fps, int brightness) {
  if (currentScreen != SCREEN_STATUS) return;

  int16_t footerY = 105;
  // Clear just the footer area
  tft.fillRect(0, footerY + 1, 240, 30, ST77XX_BLACK);
  tft.drawFastHLine(0, footerY, 240, ST77XX_WHITE);

  tft.setCursor(4, footerY + 4);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("FPS: ");
  tft.setTextColor(ST77XX_CYAN);
  tft.print(fps, 1);

  tft.setCursor(100, footerY + 4);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Bright: ");
  tft.setTextColor(ST77XX_CYAN);
  tft.print(brightness);

  tft.setCursor(180, footerY + 4);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Heap:");
  tft.print(ESP.getFreeHeap() / 1024);
  tft.print("k");
}

// =====================================================================
//  Quick single-field update: output active/idle indicator
// =====================================================================
void displayUpdateOutputActive(uint8_t index, bool active, OutputType type) {
  if (currentScreen != SCREEN_STATUS) return;

  int16_t y = 18 + index * 28;
  tft.fillRect(160, y, 80, 10, ST77XX_BLACK);
  tft.setCursor(160, y);
  tft.setTextSize(1);
  if (type == OUTPUT_OFF) {
    tft.setTextColor(0x7BEF);
    tft.print("OFF");
  } else if (active) {
    tft.setTextColor(ST77XX_GREEN);
    tft.print("RECV");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.print("IDLE");
  }
}

#endif // DISPLAY_H
