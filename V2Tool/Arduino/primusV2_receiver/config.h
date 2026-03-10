/*
 * config.h — PrimusV2 Receiver Configuration
 * ============================================
 * Output type definitions, network defaults, and hardware pin mapping.
 * Edit this file to match your physical setup.
 *
 * STAGE 2 will migrate these settings to WebGUI + NVS flash.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =====================================================================
//  Firmware Info
// =====================================================================
#define FIRMWARE_NAME    "PrimusV2"
#define FIRMWARE_VERSION "1.0.0"

// =====================================================================
//  Output Type Definitions
// =====================================================================
enum OutputType {
  OUTPUT_OFF         = 0,  // Port disabled
  OUTPUT_SHORT_STRIP = 1,  // 33-pixel RGB strip
  OUTPUT_LONG_STRIP  = 2,  // 72-pixel RGBW strip
  OUTPUT_GRID        = 3   // 4×8 (32-pixel) RGB grid
};

enum ColorOrder {
  COLOR_RGB  = 0,
  COLOR_RGBW = 1
};

// Human-readable names (for TFT / Serial)
const char* outputTypeNames[] = { "Off", "Short Strip", "Long Strip", "Grid 4x8" };
const uint16_t outputTypePixels[] = { 0, 33, 72, 32 };

// =====================================================================
//  Per-Output Configuration
// =====================================================================
#define NUM_OUTPUTS 3

struct OutputConfig {
  OutputType  type;
  ColorOrder  colorOrder;
  uint8_t     pxl8Port;     // 0, 1, or 2
  uint16_t    pixelCount;   // derived from type, but stored for convenience
  uint8_t     bytesPerPixel; // 3 for RGB, 4 for RGBW
  uint8_t     universe;     // ArtNet universe (default = port index)
};

// ----- DEFAULT OUTPUT LAYOUT -----
// Change these to match what's physically plugged in.
// Stage 2 will make this configurable at runtime via WebGUI.
inline void loadDefaultConfig(OutputConfig outputs[NUM_OUTPUTS]) {
  // Output 0 — Grid 4×8, 32 RGB LEDs (like V1 badge)
  outputs[0].type          = OUTPUT_GRID;
  outputs[0].colorOrder    = COLOR_RGB;
  outputs[0].pxl8Port      = 0;
  outputs[0].pixelCount    = 32;
  outputs[0].bytesPerPixel = 3;
  outputs[0].universe      = 0;

  // Output 1 — Short strip, 33 RGB LEDs
  outputs[1].type          = OUTPUT_SHORT_STRIP;
  outputs[1].colorOrder    = COLOR_RGB;
  outputs[1].pxl8Port      = 1;
  outputs[1].pixelCount    = 33;
  outputs[1].bytesPerPixel = 3;
  outputs[1].universe      = 1;

  // Output 2 — Long strip, 72 RGBW LEDs
  outputs[2].type          = OUTPUT_LONG_STRIP;
  outputs[2].colorOrder    = COLOR_RGBW;
  outputs[2].pxl8Port      = 2;
  outputs[2].pixelCount    = 72;
  outputs[2].bytesPerPixel = 4;
  outputs[2].universe      = 2;
}

// =====================================================================
//  NeoPXL8 Hardware
// =====================================================================
// Maximum pixels on any single port — NeoPXL8 allocates this for all 8 ports
#define MAX_LEDS_PER_PORT 72

// Pin mapping: ESP32-S3 GPIO → NeoPXL8 input port
// Ports 3-7 unused (-1)
#define PIN_PORT_0  18   // A0 / GPIO18
#define PIN_PORT_1  17   // A1 / GPIO17
#define PIN_PORT_2  16   // A2 / GPIO16

// =====================================================================
//  Buttons — ESP32-S3 Reverse TFT Feather
// =====================================================================
#define BTN_D0  0   // Active-LOW (INPUT_PULLUP)
#define BTN_D1  1   // Active-HIGH (INPUT_PULLDOWN)
#define BTN_D2  2   // Active-HIGH (INPUT_PULLDOWN)

// =====================================================================
//  Network Defaults
// =====================================================================
#define DEFAULT_WIFI_SSID      "NETGEAR44"
#define DEFAULT_WIFI_PASSWORD  "sweetgadfly251"

#define DEFAULT_STATIC_IP      192, 168, 1, 100
#define DEFAULT_GATEWAY        192, 168, 1, 1
#define DEFAULT_SUBNET         255, 255, 255, 0

// =====================================================================
//  LED Defaults
// =====================================================================
#define DEFAULT_BRIGHTNESS     50    // 0-255

// =====================================================================
//  Timing Constants (ms)
// =====================================================================
#define MIN_UPDATE_INTERVAL    16    // ~60 FPS max
#define FPS_INTERVAL           1000  // Report FPS every 1 s
#define CONNECTION_TIMEOUT     10000 // Reconnect if no packets for 10 s
#define RECONNECT_INTERVAL     5000  // Retry WiFi every 5 s
#define BRIGHTNESS_THRESHOLD   2     // Hysteresis for brightness changes

// =====================================================================
//  Brightness Presets (for D2 button cycling)
// =====================================================================
#define NUM_BRIGHTNESS_PRESETS 4
const uint8_t brightnessPresets[NUM_BRIGHTNESS_PRESETS] = { 64, 128, 191, 255 };
// Labels shown on TFT
const char* brightnessPresetNames[NUM_BRIGHTNESS_PRESETS] = { "25%", "50%", "75%", "100%" };

#endif // CONFIG_H
