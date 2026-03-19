/*
 * LED Costume Controller - Processing Control Interface
 * ------------------------------------------
 * 
 * This code provides a graphical interface to control three NeoPixel
 * outputs via Art-Net, matching the PrimusV2 hardware:
 *   Strip A0: 8×4 grid, 32 LEDs  (universe 0)
 *   Strip A1: 68 LEDs             (universe 1)
 *   Strip A2: 72 LEDs             (universe 2)
 * 
 * The interface allows for:
 * - Setting colors and animations for all LED outputs independently
 * - Adjusting brightness, speed, and animation parameters
 * - Connecting to ESP32 devices over WiFi using Art-Net protocol
 * 
 * Hardware Requirements:
 * - Computer running Processing
 * - ESP32-S3 Reverse TFT Feather + NeoPXL8 Friend running primusV2_receiver
 * - WiFi network connecting both devices
 * 
 * Libraries Required:
 * - ControlP5: https://github.com/sojamo/controlp5
 * - ArtnetP5: https://github.com/rstephan/ArtnetP5
 * 
 */

import controlP5.*;
import ch.bildspur.artnet.*;

// Global variables
ControlP5 cp5;
Grid ledGrid;
Strip ledStrip1;  // A1: 68 LEDs (universe 1)
Strip ledStrip2;  // A2: 72 LEDs (universe 2)
boolean initialized = false;

// Animation timer to track elapsed time for animations
long startTime;

// Currently selected effects and playback modes
String gridEffect = "pulse";
String stripEffect = "pulse";
String gridPlayback = "loop";
String stripPlayback = "loop";

// Current colors
color gridStartColor = #FF00FF; // Default magenta
color gridEndColor = #00FFFF;   // Default cyan
color stripStartColor = #FF0000; // Default red
color stripEndColor = #0000FF;   // Default blue

// Speed values
float gridSpeed = 1.0;
float stripSpeed = 0.5;

// Strip LED counts (matching hardwareTest.ino)
int strip1Count = 68;  // A1
int strip2Count = 72;  // A2

// Linear fade angle for grid
float gridLinearAngle = 0;

// ArtNet settings
ArtNetDevice artnetDevice;
String deviceIP = "192.168.1.100";  // Default IP address from the Arduino code
boolean artNetEnabled = true;        // Enable/disable ArtNet sending
boolean isConnected = false;         // Track connection status

// LED Brightness (0-255)
int ledBrightness = 50;  // Default value matching Arduino code

// Frame timing
int lastFrameTime = 0;
int targetFPS = 30;  // Default target FPS
int frameInterval = 1000/targetFPS;  // Frame interval in milliseconds

// Window dimensions
int windowWidth = 1080;    // Window width
int windowHeight = 1200;   // Window height

void setup() {
  size(1080, 1200);  // Window size
  background(245);
  
  // Initialize UI
  cp5 = new ControlP5(this);
  
  // Initialize LEDs — Grid (A0) must be initialized before Strips
  ledGrid = new Grid(8, 4);
  float strip1Y = ledGrid.yPos + ledGrid.height + 40;
  ledStrip1 = new Strip(strip1Count, strip1Y, "A1");
  float strip2Y = strip1Y + ledStrip1.getHeight() + 30;
  ledStrip2 = new Strip(strip2Count, strip2Y, "A2");
  
  // Initialize UI controls - must be after LED initialization
  setupControls();
  
  // Apply styling to color wheels
  fixColorWheelStyling();
  
  // Initialize animation timer
  startTime = millis();
  
  // Initialize ArtNet device (3 outputs: A0=32, A1=68, A2=72)
  if (artNetEnabled) {
    try {
      int[] ledCounts = {32, strip1Count, strip2Count};
      artnetDevice = new ArtNetDevice(deviceIP, ledCounts);
      println("ArtNet initialized: " + deviceIP + " on universes 0,1,2");
      isConnected = true;
      updateStatusLabel();
    } catch (Exception e) {
      println("Error initializing ArtNet: " + e.getMessage());
      artNetEnabled = false;
      isConnected = false;
      updateStatusLabel();
    }
  }
  
  initialized = true;
}

void draw() {
  // Frame rate limiting for all updates (animation and ArtNet)
  int currentTime = millis();
  if (currentTime - lastFrameTime < frameInterval) {
    return;
  }
  lastFrameTime = currentTime;
  
  background(245);
  
  // Draw main container
  fill(255);
  stroke(0, 0, 0, 25);
  rect(20, 20, width-40, height-40, 8);
  
  // Update animations
  if (initialized) {
    // Update color values from the ColorWheels
    updateColorsFromWheels();
    
    ledGrid.update();
    ledStrip1.update();
    ledStrip2.update();
    
    // Draw the LED grid and strips
    ledGrid.display();
    ledStrip1.display();
    ledStrip2.display();
    
    // Send the LED data via ArtNet (3 universes)
    if (artNetEnabled && artnetDevice != null && isConnected) {
      artnetDevice.updateData(ledGrid.leds, ledStrip1.leds, ledStrip2.leds, ledBrightness);
      artnetDevice.sendData();
    }
  }
}

// Reset the animation timer
void resetAnimations() {
  startTime = millis();
  ledGrid.resetColors();
  ledStrip1.resetColors();
  ledStrip2.resetColors();
}

// Update color values from ColorWheel components
void updateColorsFromWheels() {
  // Get ColorWheels from ControlP5
  ColorWheel gridStartWheel = (ColorWheel) cp5.getController("gridStartColorWheel");
  ColorWheel gridEndWheel = (ColorWheel) cp5.getController("gridEndColorWheel");
  ColorWheel stripStartWheel = (ColorWheel) cp5.getController("stripStartColorWheel");
  ColorWheel stripEndWheel = (ColorWheel) cp5.getController("stripEndColorWheel");
  
  // Update color values if wheels are available
  if (gridStartWheel != null) {
    gridStartColor = gridStartWheel.getRGB();
  }
  
  if (gridEndWheel != null) {
    gridEndColor = gridEndWheel.getRGB();
  }
  
  if (stripStartWheel != null) {
    stripStartColor = stripStartWheel.getRGB();
  }
  
  if (stripEndWheel != null) {
    stripEndColor = stripEndWheel.getRGB();
  }
}

// Fix the styling of the ColorWheel components
void fixColorWheelStyling() {
  // This function is now a placeholder since we're letting the ColorWheel handle its own layout
}

// Event handlers for controls
void controlEvent(ControlEvent event) {
  // Handle UI events
  if (!event.isController()) return;
  
  String name = event.getController().getName();
  
  // Grid effect changed
  if (name.equals("gridEffect")) {
    gridEffect = cp5.get(ScrollableList.class, "gridEffect").getItem((int)event.getValue()).get("name").toString();
    resetAnimations();
  }
  // Grid playback changed
  else if (name.equals("gridPlayback")) {
    gridPlayback = cp5.get(ScrollableList.class, "gridPlayback").getItem((int)event.getValue()).get("name").toString();
    resetAnimations();
  }
  // Strip effect changed
  else if (name.equals("stripEffect")) {
    stripEffect = cp5.get(ScrollableList.class, "stripEffect").getItem((int)event.getValue()).get("name").toString();
    resetAnimations();
  }
  // Strip playback changed
  else if (name.equals("stripPlayback")) {
    stripPlayback = cp5.get(ScrollableList.class, "stripPlayback").getItem((int)event.getValue()).get("name").toString();
    resetAnimations();
  }
  // Grid speed changed
  else if (name.equals("gridSpeed")) {
    gridSpeed = event.getController().getValue();
  }
  // Strip speed changed
  else if (name.equals("stripSpeed")) {
    stripSpeed = event.getController().getValue();
  }
  // Grid linear angle changed
  else if (name.equals("gridLinearAngle")) {
    gridLinearAngle = event.getController().getValue();
  }
  // Device IP changed
  else if (name.equals("deviceIP")) {
    deviceIP = cp5.get(Textfield.class, "deviceIP").getText();
  }
  // Connect button pressed
  else if (name.equals("connectArtNet")) {
    connectArtNet();
  }
  // Disconnect button pressed
  else if (name.equals("disconnectArtNet")) {
    disconnectArtNet();
  }
  // FPS changed
  else if (name.equals("targetFPS")) {
    targetFPS = int(event.getController().getValue());
    frameInterval = 1000/targetFPS;
  }
  // Brightness changed
  else if (name.equals("ledBrightness")) {
    ledBrightness = int(event.getController().getValue());
  }
}

// ArtNet connection methods
void connectArtNet() {
  // Make sure we get the most current IP from the text field
  deviceIP = cp5.get(Textfield.class, "deviceIP").getText();
  
  // Clean up existing connection if any
  if (artnetDevice != null) {
    artnetDevice.stop();
    artnetDevice = null;
  }
  
  try {
    // Create ArtNet device — 3 outputs on separate universes
    int[] ledCounts = {32, strip1Count, strip2Count};
    artnetDevice = new ArtNetDevice(deviceIP, ledCounts);
    artNetEnabled = true;
    isConnected = true;
    updateStatusLabel();
    println("Connected to ArtNet device at: " + deviceIP);
  } catch (Exception e) {
    println("Error connecting to ArtNet: " + e.getMessage());
    isConnected = false;
    artNetEnabled = false;
    updateStatusLabel();
  }
}

// Disconnect from ArtNet and turn off all LEDs
void disconnectArtNet() {
  if (artnetDevice != null && artnetDevice.connected) {
    println("Starting disconnect sequence...");
    
    try {
      // Use the dedicated blackout method to turn off all LEDs
      artnetDevice.sendBlackout(ledBrightness);
      
      // Wait briefly to ensure data is sent
      delay(100);
      
      // Close the connection
      artnetDevice.stop();
      artnetDevice = null;
      isConnected = false;
      artNetEnabled = false;
      updateStatusLabel();
      println("Disconnected from ArtNet device at: " + deviceIP);
    } catch (Exception e) {
      println("Error during disconnection: " + e.getMessage());
    }
  }
}

// Update the host IP display if necessary
void updateStatusLabel() {
  // We removed the status text from UI as requested
  // Update IP text color based on connection
  Textfield ipField = cp5.get(Textfield.class, "deviceIP");
  if (ipField != null) {
    if (isConnected) {
      // Add green highlight when connected
      ipField.setColorBackground(color(0, 65, 0));
    } else {
      // Default color when not connected
      ipField.setColorBackground(color(0, 45, 90));
    }
  }
}

// Clean shutdown - important for ArtNet
void exit() {
  if (artnetDevice != null) {
    // Send blackout before exiting
    artnetDevice.sendBlackout(ledBrightness);
    delay(100);
    artnetDevice.stop();
  }
  super.exit();
}
