/*
 * buttons.h — PrimusV2 Button Handler
 * =====================================
 * Handles the 3 buttons on the ESP32-S3 Reverse TFT Feather.
 *
 * D0 (active-LOW): Cycle TFT info screens (connection → status → error)
 * D1 (active-HIGH): Toggle test mode (local animations without ArtNet)
 * D2 (active-HIGH): Cycle brightness presets (25% / 50% / 75% / 100%)
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include "config.h"

// =====================================================================
//  State
// =====================================================================
bool lastBtnState[3] = { false, false, false };
const uint8_t btnPins[3] = { BTN_D0, BTN_D1, BTN_D2 };

// =====================================================================
//  Actions — set by button presses, consumed by main loop
// =====================================================================
volatile bool btnScreenCycle   = false;  // D0 pressed
volatile bool btnTestToggle    = false;  // D1 pressed
volatile bool btnBrightCycle   = false;  // D2 pressed

// =====================================================================
//  Init
// =====================================================================
void buttonsInit() {
  pinMode(BTN_D0, INPUT_PULLUP);    // D0: active-LOW
  pinMode(BTN_D1, INPUT_PULLDOWN);  // D1: active-HIGH
  pinMode(BTN_D2, INPUT_PULLDOWN);  // D2: active-HIGH
}

// =====================================================================
//  Read with polarity handling
// =====================================================================
bool readButton(uint8_t index) {
  bool raw = digitalRead(btnPins[index]);
  // D0 is active-LOW → invert; D1/D2 are active-HIGH → use as-is
  return (index == 0) ? !raw : raw;
}

// =====================================================================
//  Poll — call once per loop iteration
//  Detects rising edges and sets action flags.
// =====================================================================
void buttonsPoll() {
  for (uint8_t i = 0; i < 3; i++) {
    bool pressed = readButton(i);

    // Rising edge detection (just pressed)
    if (pressed && !lastBtnState[i]) {
      switch (i) {
        case 0: btnScreenCycle = true; break;
        case 1: btnTestToggle  = true; break;
        case 2: btnBrightCycle = true; break;
      }
    }
    lastBtnState[i] = pressed;
  }
}

#endif // BUTTONS_H
