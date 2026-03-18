/*
 * primusV2_receiver.ino — PrimusV2 Raw UDP LED Receiver
 * ======================================================
 * Receives a single raw UDP packet per frame containing all strip
 * data and drives up to 3 NeoPixel outputs via the NeoPXL8 hardware
 * buffer on an ESP32-S3 Reverse TFT Feather.
 *
 * Packet format (from sender):
 *   Bytes 0-2: "PV2" magic header
 *   Byte 3:    sequence number
 *   Bytes 4+:  RGB data, all strips concatenated in config order
 *              Strip 0: 32px × 3 = 96 bytes
 *              Strip 1: 68px × 3 = 204 bytes
 *              Strip 2: 72px × 3 = 216 bytes
 *              Total: 520 bytes per packet
 *
 * Hardware:
 *   - Adafruit ESP32-S3 Reverse TFT Feather
 *   - Adafruit NeoPXL8 Friend breakout (3.3V → 5V level shift)
 *   - Up to 3 NeoPixel strips/grids
 *
 * Libraries Required:
 *   - Adafruit_NeoPXL8
 *   - Adafruit_ST7789 + Adafruit_GFX  (for TFT)
 *
 * Board: "Adafruit Feather ESP32-S3 Reverse TFT"
 */

#include <WiFi.h>
#include <WiFiUdp.h>
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

// Raw UDP
#define UDP_PORT 6454
#define UDP_MAGIC_0 'P'
#define UDP_MAGIC_1 'V'
#define UDP_MAGIC_2 '2'
#define MAX_UDP_PACKET 600  // 4 header + 516 data max
WiFiUDP udp;
uint8_t udpBuf[MAX_UDP_PACKET];

// Per-output data buffers (max possible: 72 pixels × 4 bytes RGBW = 288 bytes)
#define MAX_BUFFER_SIZE (MAX_LEDS_PER_PORT * 4)
uint8_t outputBuffers[NUM_OUTPUTS][MAX_BUFFER_SIZE];
bool    outputDataReady[NUM_OUTPUTS]  = { false, false, false };
bool    outputActive[NUM_OUTPUTS]     = { false, false, false };
unsigned long outputLastPacket[NUM_OUTPUTS] = { 0, 0, 0 };

// Brightness — fixed at 255 (sender sends raw RGB values).
// D2 button cycles brightness only in local test mode.
int currentBrightness = 255;

// WiFi state
bool wifiConnected = false;
unsigned long lastReconnectAttempt = 0;

// Timing
unsigned long lastShowTime = 0;
#define SHOW_INTERVAL 16  // ~60 FPS max show rate
unsigned long lastFpsTime = 0;
unsigned long frameCount = 0;
unsigned long packetCount = 0;
float currentFps = 0;
bool newDataSinceLastShow = false;

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
          udp.begin(UDP_PORT);
          Serial.println("UDP reinitialized after reconnection.");
        }
      }
    }
  }
}

// =====================================================================
//  Raw UDP Packet Handler
// =====================================================================

void processRawPacket(uint8_t* data, uint16_t len) {
  // Validate magic header: "PV2" at bytes 0-2
  if (len < 4) return;
  if (data[0] != UDP_MAGIC_0 || data[1] != UDP_MAGIC_1 || data[2] != UDP_MAGIC_2) return;

  // Byte 3 = sequence (informational)
  // Bytes 4+ = concatenated RGB data for all strips in config order
  uint16_t dataLen = len - 4;
  uint8_t* pixelData = data + 4;

  unsigned long now = millis();
  packetCount++;

  // Walk through outputs, slicing the concatenated data by pixel count
  uint16_t offset = 0;
  for (uint8_t o = 0; o < NUM_OUTPUTS; o++) {
    if (outputs[o].type == OUTPUT_OFF) continue;

    uint16_t pixelCount = outputs[o].pixelCount;
    uint8_t  bpp        = outputs[o].bytesPerPixel;
    uint16_t stripBytes = pixelCount * bpp;

    if (offset + stripBytes > dataLen) break;  // Not enough data

    // Copy into output buffer
    memcpy(outputBuffers[o], pixelData + offset, stripBytes);
    outputDataReady[o] = true;
    outputActive[o] = true;
    outputLastPacket[o] = now;

    offset += stripBytes;
  }

  newDataSinceLastShow = true;
}

// =====================================================================
//  LED Update — apply buffered data to NeoPXL8
// =====================================================================

void applyBufferedData() {
  // Apply any pending output buffers to the NeoPXL8 pixel array.
  // Does NOT call show() — that happens on a fixed timer in loop().
  for (uint8_t o = 0; o < NUM_OUTPUTS; o++) {
    if (!outputDataReady[o]) continue;

    uint8_t  port  = outputs[o].pxl8Port;
    uint16_t count = outputs[o].pixelCount;
    uint8_t  bpp   = outputs[o].bytesPerPixel;

    for (uint16_t p = 0; p < count; p++) {
      uint16_t base = p * bpp;

      if (bpp == 4) {
        setStripPixel(port, p, Adafruit_NeoPixel::Color(
          outputBuffers[o][base],
          outputBuffers[o][base + 1],
          outputBuffers[o][base + 2],
          outputBuffers[o][base + 3]
        ));
      } else {
        setStripPixel(port, p, Adafruit_NeoPixel::Color(
          outputBuffers[o][base],
          outputBuffers[o][base + 1],
          outputBuffers[o][base + 2]
        ));
      }
    }

    outputDataReady[o] = false;
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
  // Only affects local test mode — remote mode uses raw RGB values
  brightPresetIndex = (brightPresetIndex + 1) % NUM_BRIGHTNESS_PRESETS;
  currentBrightness = brightnessPresets[brightPresetIndex];
  if (testModeActive) {
    leds->setBrightness(currentBrightness);
  }
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

  leds->setBrightness(255);  // Always full — sender pre-multiplies brightness
  leds->fill(0);
  leds->show();
  Serial.println("NeoPXL8 initialized OK");

  // Connect WiFi
  Serial.println("Connecting to WiFi...");
  wifiConnected = connectWifi();

  if (wifiConnected) {
    displayConnection(DEFAULT_WIFI_SSID, WiFi.localIP(), true, WiFi.RSSI());
  } else {
    displayError("WiFi Fail", "Could not connect. Retrying...");
  }

  // Init raw UDP listener
  udp.begin(UDP_PORT);
  Serial.print("Raw UDP listening on port ");
  Serial.println(UDP_PORT);

  // Init timing
  lastFpsTime = millis();
  lastShowTime = millis();

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
    // In test mode — run local animations, skip UDP processing
    runTestAnimations();
    delay(33); // ~30fps
    return;
  }

  // Normal operation — process raw UDP
  checkWifiConnection();

  // Drain ALL pending UDP packets
  int pktSize;
  while ((pktSize = udp.parsePacket()) > 0) {
    if (pktSize > MAX_UDP_PACKET) {
      // Too large — flush it
      while (udp.available()) udp.read();
      continue;
    }
    int bytesRead = udp.read(udpBuf, pktSize);
    if (bytesRead > 0) {
      processRawPacket(udpBuf, bytesRead);
    }
  }

  // Apply any received data to the pixel buffer
  applyBufferedData();

  // Fixed-rate show — pushes pixels at a steady rate, never mid-frame
  if (newDataSinceLastShow && (now - lastShowTime >= SHOW_INTERVAL)) {
    leds->show();
    lastShowTime = now;
    newDataSinceLastShow = false;
    frameCount++;
  }

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

    // Debug: print first pixel of each active output
    for (uint8_t o = 0; o < NUM_OUTPUTS; o++) {
      if (outputActive[o] && outputs[o].pixelCount > 0) {
        Serial.print("  Out");
        Serial.print(o);
        Serial.print(" px0: R=");
        Serial.print(outputBuffers[o][0]);
        Serial.print(" G=");
        Serial.print(outputBuffers[o][1]);
        Serial.print(" B=");
        Serial.println(outputBuffers[o][2]);
      }
    }

    // Update TFT footer if on status screen
    displayUpdateFooter(currentFps, currentBrightness);

    frameCount = 0;
    packetCount = 0;
    lastFpsTime = now;
  }
}
