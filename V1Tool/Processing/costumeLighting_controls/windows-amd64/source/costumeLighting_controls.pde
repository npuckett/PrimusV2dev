/*
 * LED Costume Controller - Processing Control Interface
 * ------------------------------------------
 * 
 * This code provides a graphical interface to control two NeoPixel LED strips via Art-Net:
 * 1. A badge (8x4 grid, 32 LEDs)
 * 2. A collar (30 LEDs by default, configurable)
 * 
 * The interface allows for:
 * - Setting colors and animations for both LED sets independently
 * - Adjusting brightness, speed, and animation parameters
 * - Connecting to ESP32 devices over WiFi using Art-Net protocol
 * 
 * Hardware Requirements:
 * - Computer running Processing
 * - ESP32 device running the companion receiver sketch
 * - WiFi network connecting both devices
 * 
 * Libraries Required:
 * - ControlP5: https://github.com/sojamo/controlp5
 * - ArtnetP5: https://github.com/rstephan/ArtnetP5
 * 
 * Settings to Change:
 * - Default device IP address if needed
 * - Default LED counts and effects if desired
 * 
 */

import controlP5.*;
import ch.bildspur.artnet.*;

// Global variables
ControlP5 cp5;
Grid ledGrid;
Strip ledStrip;
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

// Strip LED count
int stripCount = 30;

// Linear fade angle for grid
float gridLinearAngle = 0;

// ArtNet settings
ArtNetDevice artnetDevice;
String deviceIP = "192.168.1.100";  // Default IP address from the Arduino code
int universe = 0;                    // Default universe (hardcoded now)
boolean artNetEnabled = true;        // Enable/disable ArtNet sending
boolean isConnected = false;         // Track connection status
boolean useV2Protocol = false;       // false = V1 single universe, true = V2 multi-universe

// LED Brightness (0-255)
int ledBrightness = 50;  // Default value matching Arduino code

// Frame timing
int lastFrameTime = 0;
int targetFPS = 30;  // Default target FPS
int frameInterval = 1000/targetFPS;  // Frame interval in milliseconds

// Window dimensions
int windowWidth = 1080;    // Window width
int windowHeight = 1000;   // Window height

void setup() {
  size(1080, 1100);  // Window size
  background(245);
  
  // Initialize UI
  cp5 = new ControlP5(this);
  
  // Initialize LEDs - Grid must be initialized before Strip
  ledGrid = new Grid(8, 4);
  ledStrip = new Strip(stripCount);
  
  // Initialize UI controls - must be after LED initialization
  setupControls();
  
  // Apply styling to color wheels
  fixColorWheelStyling();
  
  // Initialize animation timer
  startTime = millis();
  
  // Initialize ArtNet device (badge = 32 LEDs, collar = 30 LEDs by default)
  if (artNetEnabled) {
    try {
      artnetDevice = new ArtNetDevice(universe, deviceIP, 32 + stripCount, useV2Protocol);
      println("ArtNet initialized: " + deviceIP + " on universe " + universe);
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
    ledStrip.update();
    
    // Draw the LED grid and strip
    ledGrid.display();
    ledStrip.display();
    
    // Send the LED data via ArtNet
    if (artNetEnabled && artnetDevice != null && isConnected) {
      artnetDevice.updateData(ledGrid.leds, ledStrip.leds, ledBrightness);
      artnetDevice.sendData();
    }
  }
}

// Reset the animation timer
void resetAnimations() {
  startTime = millis();
  ledGrid.resetColors();
  ledStrip.resetColors();
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
  // Strip count changed
  else if (name.equals("stripCount")) {
    stripCount = int(event.getController().getValue());
    ledStrip = new Strip(stripCount);
    // Recreate ArtNet device with new LED count
    if (artNetEnabled && isConnected) {
      connectArtNet();
    }
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
  // Device profile changed (V1 legacy vs V2 multi-universe)
  else if (name.equals("deviceProfile")) {
    int profileIdx = int(event.getController().getValue());
    useV2Protocol = (profileIdx == 1);
    println("Device profile → " + (useV2Protocol ? "V2 Multi-Universe" : "V1 Legacy"));
    // Reconnect with new protocol if currently connected
    if (artNetEnabled && isConnected) {
      connectArtNet();
    }
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
    // Create ArtNet device — V2 mode sends grid/strip on separate universes
    artnetDevice = new ArtNetDevice(0, deviceIP, 32 + stripCount, useV2Protocol);
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
