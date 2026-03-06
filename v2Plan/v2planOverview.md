## General Plan
This prototype is a continuation of previous work that looks at streaming data over wifi to ESP32 devices that then output that data onto neopixels.

## Hardware Setup

### Microcontroller
We are using the ESP32-S3 from Adafruit that has a TFT Screen and 3 User buttons on the reverse side. We will be using both the screen and buttons in th final version to display IP info, connection status, data, etc.
Board Info : https://learn.adafruit.com/esp32-s3-reverse-tft-feather?view=all

### Hardware Buffer
To make the control more robust we are using the adafruit NeoPXL8 board that boosts the 3.3V signal to 5V and provides a hardware buffer.
Board Info : https://learn.adafruit.com/adafruit-neopxl8-featherwing-and-library/neopxl8-breakout-board

### Library
The PXL8 also has its own library : https://github.com/adafruit/Adafruit_NeoPXL8


## Intial Test Prototype
For this initial prototype we will not be worrying about the live data input over Wifi and just focussing on testing the hardware setup. It will be controlling 3 neopixel strips that are each 30 pixels long


## Connection
You can see the pinout diagram here supportImages/adafruit_products_Adafruit_Reverse_TFT_Feather_ESP32-S2_Pinout.png

Strip 1 : Control Pin: A0/GPIO18 -> PXL8 In Port: 0 -> PXL8 Out Port: 0 -> Neo Pixel
Strip 2 : Control Pin: A1/GPIO17 -> PXL8 In Port: 1 -> PXL8 Out Port: 1 -> Neo Pixel
Strip 3 : Control Pin: A2/GPIO16 -> PXL8 In Port: 2 -> PXL8 Out Port: 2 -> Neo Pixel