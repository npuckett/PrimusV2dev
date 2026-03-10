/*
 * primusV2_receiver.ino — PrimusV2 ArtNet LED Receiver
 * =====================================================
 * Receives Art-Net data over WiFi and drives up to 3 NeoPixel outputs
 * via the NeoPXL8 hardware buffer on an ESP32-S3 Reverse TFT Feather.
 *
 * Each output maps to its own ArtNet universe:
 *   Universe 0 → Output 0 (NeoPXL8 port 0)
 *   Universe 1 → Output 1 (NeoPXL8 port 1)
 *   Universe 2 → Output 2 (NeoPXL8 port 2)
 *
 * Per-universe byte layout:
 *   [brightness] [R G B (W)] [R G B (W)] ...
 *   First byte = brightness (0-255), rest = pixel data.
 *   3 bytes/pixel for RGB, 4 bytes/pixel for RGBW.
 *
 * Hardware:
 *   - Adafruit ESP32-S3 Reverse TFT Feather
 *   - Adafruit NeoPXL8 Friend breakout (3.3V → 5V level shift)
 *   - Up to 3 NeoPixel strips/grids
 *
 * Libraries Required:
 *   - ArtnetWifi   https://github.com/rstephan/ArtnetWifi
 *   - Adafruit_NeoPXL8
 *   - Adafruit_ST7789 + Adafruit_GFX  (for TFT)
 *
 * Board: "Adafruit Feather ESP32-S3 Reverse TFT"
 */

#include <WiFi.h>
#include <ArtnetWifi.h>
#include <Adafruit_NeoPXL8.h>

#include "config.h"
#include "display.h"
#include "buttons.h"

// =====================================================================
//  Globals
// =====================================================================

// Output configuration (loaded from defaults; Stage 2 → WebGUI/NVS)
OutputConfig outputs[NUM_OUTPUTS];

// NeoPXL8 pin map (8 entries, -1 = unused)
int8_t pxl8Pins[8] = {
  PIN_PORT_0, PIN_PORT_1, PIN_PORT_2,
  -1, -1, -1, -1, -1
};

// NeoPXL8 LED driver — initialized in setup after config is loaded
Adafruit_NeoPXL8* leds = nullptr;

// ArtNet
ArtnetWifi artnet;

// Per-output data buffers (max possible: 72 pixels × 4 bytes RGBW = 288 bytes)
#define MAX_BUFFER_SIZE (MAX_LEDS_PER_PORT * 4)
uint8_t outputBuffers[NUM_OUTPUTS][MAX_BUFFER_SIZE];
bool    outputDataReady[NUM_OUTPUTS]  = { false, false, false };
bool    outputActive[NUM_OUTPUTS]     = { false, false, false };
unsigned long outputLastPacket[NUM_OUTPUTS] = { 0, 0, 0 };

// Brightness
int currentBrightness = DEFAULT_BRIGHTNESS;
bool brightnessChanged = false;

// Last-color tracking for change detection (per pixel, per output)
uint32_t lastColors[NUM_OUTPUTS][MAX_LEDS_PER_PORT];

// WiFi state
bool wifiConnected = false;
unsigned long lastReconnectAttempt = 0;

// Timing
unsigned long lastLEDUpdateTime = 0;
unsigned long lastFpsTime = 0;
unsigned long frameCount = 0;
unsigned long packetCount = 0;
float currentFps = 0;

// Test mode
bool testModeActive = false;
uint8_t testModeIndex = 0;  // 0=Off, 1=Wipe, 2=White, 3=Rainbow, 4=March
#define NUM_TEST_MODES 5
const char* testModeNames[NUM_TEST_MODES] = { "Off", "Color Wipe", "White", "Rainbow", "March" };
// Test animation state (per output)
long    rainbowHue[NUM_OUTPUTS]  = { 0, 0, 0 };
uint16_t marchPos[NUM_OUTPUTS]   = { 0, 0, 0 };
uint16_t wipePos[NUM_OUTPUTS]    = { 0, 0, 0 };
bool     wipeDone[NUM_OUTPUTS]   = { false, false, false };

// Screen cycling
uint8_t infoScreenIndex = 0; // 0=connection, 1=status, 2=error

// Brightness preset index
uint8_t brightPresetIndex = 0;

// =====================================================================
//  NeoPXL8 Helpers
// =====================================================================

void setStripPixel(uint8_t port, uint16_t pixel, uint32_t color) {
  leds->setPixelColor(port * MAX_LEDS_PER_PORT + pixel, color);
}

void clearPort(uint8_t port, uint16_t count) {
  for (uint16_t p = 0; p < count; p++) {
    setStripPixel(port, p, 0);
  }
}

// =====================================================================
//  WiFi
// =====================================================================

bool connectWifi() {
  IPAddress localIP(DEFAULT_STATIC_IP);
  IPAddress gateway(DEFAULT_GATEWAY);
  IPAddress subnet(DEFAULT_SUBNET);

  WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);
  WiFi.config(localIP, gateway, subnet);
  WiFi.setSleep(false);

  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("WiFi connection failed.");
  return false;
}

void checkWifiConnection() {
  unsigned long now = millis();

  // Find the most recent packet across all outputs
  unsigned long newest = 0;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    if (outputLastPacket[i] > newest) newest = outputLastPacket[i];
  }

  if (now - newest > CONNECTION_TIMEOUT && newest > 0) {
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
        Serial.println("Reconnecting WiFi...");
        lastReconnectAttempt = now;
        wifiConnected = connectWifi();
        if (wifiConnected) {
          artnet.begin();
          Serial.println("ArtNet reinitialized after reconnection.");
        }
      }
    }
  }
}

// =====================================================================
//  ArtNet Callback
// =====================================================================

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data) {
  unsigned long now = millis();
  packetCount++;

  // Find which output this universe belongs to
  int outputIdx = -1;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    if (outputs[i].universe == universe && outputs[i].type != OUTPUT_OFF) {
      outputIdx = i;
      break;
    }
  }
  if (outputIdx < 0) return; // Unknown universe, ignore

  // Rate limiting
  if (now - lastLEDUpdateTime < MIN_UPDATE_INTERVAL) return;

  if (length == 0) return;

  outputLastPacket[outputIdx] = now;
  outputActive[outputIdx] = true;

  // First byte = brightness
  int newBrightness = data[0];
  if (abs(newBrightness - currentBrightness) > BRIGHTNESS_THRESHOLD) {
    currentBrightness = newBrightness;
    brightnessChanged = true;
  }

  // Copy pixel data into output buffer
  uint16_t pixelCount = outputs[outputIdx].pixelCount;
  uint8_t  bpp        = outputs[outputIdx].bytesPerPixel;
  uint16_t maxBytes   = min((uint16_t)(length - 1), (uint16_t)(pixelCount * bpp));

  for (uint16_t i = 0; i < maxBytes; i++) {
    outputBuffers[outputIdx][i] = data[1 + i];
  }

  outputDataReady[outputIdx] = true;
  lastLEDUpdateTime = now;
}

// =====================================================================
//  LED Update — apply buffered data to NeoPXL8
// =====================================================================

void updateLEDs() {
  if (brightnessChanged) {
    leds->setBrightness(currentBrightness);
    brightnessChanged = false;
  }

  bool anyUpdate = false;

  for (uint8_t o = 0; o < NUM_OUTPUTS; o++) {
    if (!outputDataReady[o]) continue;

    uint8_t  port  = outputs[o].pxl8Port;
    uint16_t count = outputs[o].pixelCount;
    uint8_t  bpp   = outputs[o].bytesPerPixel;

    for (uint16_t p = 0; p < count; p++) {
      uint32_t newColor;
      uint16_t base = p * bpp;

      if (bpp == 4) {
        // RGBW
        newColor = Adafruit_NeoPixel::Color(
          outputBuffers[o][base],
          outputBuffers[o][base + 1],
          outputBuffers[o][base + 2],
          outputBuffers[o][base + 3]
        );
      } else {
        // RGB
        newColor = Adafruit_NeoPixel::Color(
          outputBuffers[o][base],
          outputBuffers[o][base + 1],
          outputBuffers[o][base + 2]
        );
      }

      // Change detection — only update if color differs
      if (newColor != lastColors[o][p]) {
        setStripPixel(port, p, newColor);
        lastColors[o][p] = newColor;
        anyUpdate = true;
      }
    }

    outputDataReady[o] = false;
  }

  if (anyUpdate) {
    leds->show();
    frameCount++;
  }
}

// =====================================================================
//  Test Mode Animations (from hardwareTest.ino)
// =====================================================================

uint32_t testColor(uint8_t o) {
  switch (o) {
    case 0:  return Adafruit_NeoPixel::Color(255, 0, 0);
    case 1:  return Adafruit_NeoPixel::Color(0, 255, 0);
    case 2:  return Adafruit_NeoPixel::Color(0, 0, 255);
    default: return Adafruit_NeoPixel::Color(255, 255, 255);
  }
}

void runTestAnimations() {
  for (uint8_t o = 0; o < NUM_OUTPUTS; o++) {
    if (outputs[o].type == OUTPUT_OFF) continue;
    uint8_t  port  = outputs[o].pxl8Port;
    uint16_t count = outputs[o].pixelCount;

    switch (testModeIndex) {
      case 0: // Off
        clearPort(port, count);
        break;
      case 1: // Color Wipe
        if (!wipeDone[o] && wipePos[o] < count) {
          setStripPixel(port, wipePos[o], testColor(o));
          wipePos[o]++;
        } else {
          wipeDone[o] = true;
        }
        break;
      case 2: // White
        for (uint16_t p = 0; p < count; p++) {
          setStripPixel(port, p, Adafruit_NeoPixel::Color(255, 255, 255));
        }
        break;
      case 3: // Rainbow
        for (uint16_t p = 0; p < count; p++) {
          uint16_t hue = rainbowHue[o] + (p * 65536L / count);
          setStripPixel(port, p, Adafruit_NeoPixel::ColorHSV(hue, 255, 255));
        }
        rainbowHue[o] += 512;
        if (rainbowHue[o] >= 65536) rainbowHue[o] -= 65536;
        break;
      case 4: // March
        clearPort(port, count);
        setStripPixel(port, marchPos[o], testColor(o));
        marchPos[o] = (marchPos[o] + 1) % count;
        break;
    }
  }
  leds->show();
}

void resetTestState() {
  for (uint8_t o = 0; o < NUM_OUTPUTS; o++) {
    rainbowHue[o] = 0;
    marchPos[o] = 0;
    wipePos[o] = 0;
    wipeDone[o] = false;
    if (outputs[o].type != OUTPUT_OFF) {
      clearPort(outputs[o].pxl8Port, outputs[o].pixelCount);
    }
  }
  leds->show();
}

// =====================================================================
//  Button Action Handlers
// =====================================================================

void handleScreenCycle() {
  infoScreenIndex = (infoScreenIndex + 1) % NUM_INFO_SCREENS;
  switch (infoScreenIndex) {
    case 0:
      displayConnection(DEFAULT_WIFI_SSID, WiFi.localIP(), wifiConnected,
                        wifiConnected ? WiFi.RSSI() : 0);
      break;
    case 1:
      displayStatus(outputs, currentFps, currentBrightness, outputActive);
      break;
    case 2:
      if (!wifiConnected) {
        displayError("WiFi Lost", "Attempting reconnection...");
      } else {
        displayError("No Errors", "System running normally");
      }
      break;
  }
  Serial.print("Screen → ");
  Serial.println(infoScreenIndex);
}

void handleTestToggle() {
  if (!testModeActive) {
    // Enter test mode
    testModeActive = true;
    testModeIndex = 1; // Start on first real animation
    resetTestState();
    displayTestMode(testModeIndex, testModeNames[testModeIndex]);
    Serial.println("Entering test mode");
  } else {
    // Cycle through test modes; wrapping past last exits test mode
    testModeIndex++;
    if (testModeIndex >= NUM_TEST_MODES) {
      // Exit test mode
      testModeActive = false;
      testModeIndex = 0;
      resetTestState();
      // Return to normal screen
      handleScreenCycle();
      Serial.println("Exiting test mode");
    } else {
      resetTestState();
      displayTestMode(testModeIndex, testModeNames[testModeIndex]);
      Serial.print("Test mode → ");
      Serial.println(testModeNames[testModeIndex]);
    }
  }
}

void handleBrightnessCycle() {
  brightPresetIndex = (brightPresetIndex + 1) % NUM_BRIGHTNESS_PRESETS;
  currentBrightness = brightnessPresets[brightPresetIndex];
  leds->setBrightness(currentBrightness);
  Serial.print("Brightness → ");
  Serial.print(brightnessPresetNames[brightPresetIndex]);
  Serial.print(" (");
  Serial.print(currentBrightness);
  Serial.println(")");

  // Quick update on TFT if on status screen
  if (currentScreen == SCREEN_STATUS) {
    displayUpdateFooter(currentFps, currentBrightness);
  }
}

// =====================================================================
//  Output idle detection
// =====================================================================
void checkOutputTimeouts() {
  unsigned long now = millis();
  for (uint8_t o = 0; o < NUM_OUTPUTS; o++) {
    if (outputs[o].type == OUTPUT_OFF) continue;
    if (outputActive[o] && (now - outputLastPacket[o] > CONNECTION_TIMEOUT)) {
      outputActive[o] = false;
      displayUpdateOutputActive(o, false, outputs[o].type);
    }
  }
}

// =====================================================================
//  Setup
// =====================================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("=============================");
  Serial.println(FIRMWARE_NAME);
  Serial.print("Firmware v"); Serial.println(FIRMWARE_VERSION);
  Serial.println("=============================");

  // Load output config
  loadDefaultConfig(outputs);

  Serial.println("Output configuration:");
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    Serial.print("  Port ");
    Serial.print(outputs[i].pxl8Port);
    Serial.print(": ");
    Serial.print(outputTypeNames[outputs[i].type]);
    Serial.print(", ");
    Serial.print(outputs[i].pixelCount);
    Serial.print("px, ");
    Serial.print(outputs[i].colorOrder == COLOR_RGBW ? "RGBW" : "RGB");
    Serial.print(", Universe ");
    Serial.println(outputs[i].universe);
  }

  // Init buttons
  buttonsInit();

  // Init TFT
  displayInit();
  displayStartup();

  // Init NeoPXL8
  // Determine NeoPixel type — use RGBW if any output needs it
  bool needsRGBW = false;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    if (outputs[i].colorOrder == COLOR_RGBW) {
      needsRGBW = true;
      break;
    }
  }

  neoPixelType pixelType = needsRGBW ? (NEO_GRBW + NEO_KHZ800) : (NEO_GRB + NEO_KHZ800);
  leds = new Adafruit_NeoPXL8(MAX_LEDS_PER_PORT, pxl8Pins, pixelType);

  if (!leds->begin()) {
    Serial.println("ERROR: NeoPXL8 begin() failed!");
    displayError("PXL8 FAIL", "NeoPXL8 initialization failed");
    while (1) { delay(100); }
  }

  leds->setBrightness(currentBrightness);
  leds->fill(0);
  leds->show();
  Serial.println("NeoPXL8 initialized OK");

  // Clear last-color tracking
  memset(lastColors, 0, sizeof(lastColors));

  // Connect WiFi
  Serial.println("Connecting to WiFi...");
  wifiConnected = connectWifi();

  if (wifiConnected) {
    displayConnection(DEFAULT_WIFI_SSID, WiFi.localIP(), true, WiFi.RSSI());
  } else {
    displayError("WiFi Fail", "Could not connect. Retrying...");
  }

  // Init ArtNet
  artnet.begin();
  artnet.setArtDmxCallback(onDmxFrame);
  Serial.println("ArtNet listening");

  // Init timing
  lastFpsTime = millis();
  lastLEDUpdateTime = millis();

  Serial.println("Setup complete. Buttons: D0=Screen D1=Test D2=Brightness");
  Serial.println();
}

// =====================================================================
//  Main Loop
// =====================================================================

void loop() {
  unsigned long now = millis();

  // Poll buttons
  buttonsPoll();

  // Handle button actions
  if (btnScreenCycle) {
    btnScreenCycle = false;
    handleScreenCycle();
  }
  if (btnTestToggle) {
    btnTestToggle = false;
    handleTestToggle();
  }
  if (btnBrightCycle) {
    btnBrightCycle = false;
    handleBrightnessCycle();
  }

  if (testModeActive) {
    // In test mode — run local animations, skip ArtNet processing
    runTestAnimations();
    delay(33); // ~30fps
    return;
  }

  // Normal operation — process ArtNet
  checkWifiConnection();
  artnet.read();

  // Apply received LED data
  updateLEDs();

  // Check for idle outputs
  checkOutputTimeouts();

  // FPS reporting
  if (now - lastFpsTime >= FPS_INTERVAL) {
    currentFps = frameCount * 1000.0 / (now - lastFpsTime);
    float packetFps = packetCount * 1000.0 / (now - lastFpsTime);

    Serial.print("FPS - LED: ");
    Serial.print(currentFps, 1);
    Serial.print(", Packets: ");
    Serial.print(packetFps, 1);
    Serial.print(", Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.print("B, RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.print("dBm, Bright: ");
    Serial.println(currentBrightness);

    // Update TFT footer if on status screen
    displayUpdateFooter(currentFps, currentBrightness);

    frameCount = 0;
    packetCount = 0;
    lastFpsTime = now;
  }
}
