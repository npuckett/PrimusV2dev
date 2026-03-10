// Controls.pde
// Functions for setting up and managing UI controls

void setupControls() {
  // Create Network Controls group at the top of the page
  setupNetworkControlGroup(40);
  
  // Calculate total height needed for LEDs to position controls below
  float ledsHeight = ledGrid.yPos + ledGrid.height + 10;
  
  // Add space for the LED strip (account for multiple rows)
  int maxLedsPerRow = (int) Math.floor((windowWidth - 100) / (ledStrip.ledSize + ledStrip.spacing));
  int rowCount = (int) Math.ceil((float)stripCount / maxLedsPerRow);
  float stripHeight = rowCount * (ledStrip.ledSize + ledStrip.spacing) + 10;
  
  // Position brightness slider centered under the strip with more space
  setupBrightnessSlider(ledStrip.yPos + stripHeight + 20);
  
  // Position controls directly after the brightness slider - with MORE space
  float controlsY = ledStrip.yPos + stripHeight + 100; // Increased spacing
  
  // Create groups for grid and strip controls with proper spacing
  setupGridControlGroup(controlsY);
  setupStripControlGroup(controlsY);
  
  // Create debug group at the bottom - with more space and collapsible
  setupDebugGroup(controlsY + 450); // Moved further down
}

void setupNetworkControlGroup(float yPos) {
  // Create a group for Network controls at the top
  Group networkGroup = cp5.addGroup("Network Controls")
    .setPosition(20, yPos)
    .setWidth(windowWidth - 40)
    .setBackgroundHeight(90) // Slightly taller to accommodate larger input
    .setBackgroundColor(color(240))
    .setBarHeight(25)
    .setColorBackground(color(60, 80, 120))
    .setColorLabel(color(255))
    .setColorActive(color(255, 128, 0))
    .disableCollapse()
    ;
    
  // IP address field - LARGER with bigger text and white text - fix flicker issue
  cp5.addTextfield("deviceIP")
     .setPosition(40, 40)
     .setSize(280, 40) // Larger size
     .setText(deviceIP)
     .setFont(createFont("Arial Bold", 18)) // Fixed font
     .setLabel("")  // Remove inline label to fix flicker
     .setLabelVisible(false) // Hide label during drawing
     .setColor(color(255)) // White text inside the box
     .setColorBackground(color(0, 45, 90))
     .setColorForeground(color(0, 55, 100))
     .setColorActive(color(0, 65, 120))
     .setGroup(networkGroup)
     .setAutoClear(false)
     ;
  
     
  // Connect button
  cp5.addButton("connectArtNet")
     .setPosition(340, 40)
     .setSize(100, 40) // Taller to match IP input
     .setLabel("CONNECT")
     .setFont(createFont("Arial", 14))
     .setColorBackground(color(0, 45, 90))
     .setColorForeground(color(0, 55, 100))
     .setColorActive(color(0, 65, 120))
     .setGroup(networkGroup)
     ;
     
  // Disconnect button - to the right of Connect button
  cp5.addButton("disconnectArtNet")
     .setPosition(450, 40)
     .setSize(120, 40) // Taller to match IP input, wider for text
     .setLabel("DISCONNECT")
     .setFont(createFont("Arial", 14))
     .setColorBackground(color(90, 45, 0)) // Different color to indicate caution
     .setColorForeground(color(100, 55, 0))
     .setColorActive(color(120, 65, 0))
     .setGroup(networkGroup)
     ;
  
  // Controller IP address display - now with larger text
  cp5.addTextlabel("hostIPDisplay")
     .setPosition(720, 50) // Shifted right to make room for device profile dropdown
     .setFont(createFont("Arial Bold", 14))
     .setText("HOST: " + getHostIP())
     .setColorValue(color(0))
     .setGroup(networkGroup)
     ;

  // Device profile toggle — V1 (single universe) vs V2 (multi-universe)
  cp5.addScrollableList("deviceProfile")
     .setPosition(580, 40)
     .setSize(120, 60)
     .setBarHeight(25)
     .setItemHeight(25)
     .addItems(java.util.Arrays.asList("V1 Legacy", "V2 Multi-Uni"))
     .setValue(0) // Default to V1 legacy for backward compatibility
     .setLabel("DEVICE")
     .setColorBackground(color(60, 80, 60))
     .setColorForeground(color(70, 90, 70))
     .setColorActive(color(80, 100, 80))
     .setGroup(networkGroup)
     ;
}

void setupBrightnessSlider(float yPos) {
  // Create a centered brightness slider with label position matching screenshot
  cp5.addSlider("ledBrightness")
     .setPosition((windowWidth - 400) / 2, yPos)
     .setSize(400, 20)
     .setRange(0, 255)
     .setValue(ledBrightness)
     .setLabelVisible(true)
     .setLabel("LED BRIGHTNESS")
     .setColorBackground(color(0, 45, 90))
     .setColorForeground(color(0, 55, 100))
     .setColorActive(color(0, 65, 120))
     .setColorLabel(color(0))
     .setColorValue(color(255))
     ;
  
  // Position the label below the slider centered
  Slider brightnessSlider = (Slider)cp5.getController("ledBrightness");
  brightnessSlider.getCaptionLabel()
                 .align(ControlP5.CENTER, ControlP5.BOTTOM_OUTSIDE)
                 .setPaddingY(5);
}

void setupGridControlGroup(float yPos) {
  // Adjusted width to fit within narrower window with 5px margin
  int groupWidth = (windowWidth - 40) / 2 - 10;
  
  // Create a group for grid controls - matching screenshot style
  Group gridGroup = cp5.addGroup("Grid Controls")
    .setPosition(20, yPos)  
    .setWidth(groupWidth)
    .setBackgroundHeight(400)
    .setBackgroundColor(color(250))
    .setBarHeight(25)
    .setColorBackground(color(60, 60, 100))
    .setColorLabel(color(255))
    .setColorActive(color(255, 128, 0))
    .disableCollapse()
    ;
    
  // Keep original wheel size
  int wheelSize = int(groupWidth * 0.4);
  
  // Original positions as per screenshot
  cp5.addColorWheel("gridStartColorWheel")
    .setPosition(30, 35)
    .setSize(wheelSize, wheelSize)
    .setRGB(gridStartColor)
    .setLabel("START COLOR")
    .setGroup(gridGroup)
    ;
    
  // End color wheel
  cp5.addColorWheel("gridEndColorWheel")
    .setPosition(wheelSize + 70, 35)
    .setSize(wheelSize, wheelSize)
    .setRGB(gridEndColor)
    .setLabel("END COLOR")
    .setGroup(gridGroup)
    ;
  
  int controlWidth = groupWidth - 40;
  int controlHalfWidth = (controlWidth - 20) / 2;
  
  // Effect dropdown - style matching screenshot
  cp5.addScrollableList("gridEffect")
    .setPosition(20, wheelSize + 45)
    .setSize(controlHalfWidth, 140)
    .setBarHeight(20)
    .setItemHeight(20)
    .addItems(java.util.Arrays.asList("none", "solid", "pulse", "linear", "constrainbow", "rainbow", "wipe"))
    .setValue(2) // Default to pulse (index 2)
    .setLabel("EFFECT")
    .setColorBackground(color(0, 45, 90))
    .setColorForeground(color(0, 55, 100))
    .setColorActive(color(0, 65, 120))
    .setGroup(gridGroup)
    ;
  
  // Linear fade angle slider - style matching screenshot
  cp5.addSlider("gridLinearAngle")
    .setPosition(controlHalfWidth + 40, wheelSize + 45)
    .setSize(controlHalfWidth, 20)
    .setRange(0, 360)
    .setValue(0)
    .setLabel("LINEAR FADE ANGLE")
    .setColorBackground(color(0, 45, 90))
    .setColorForeground(color(0, 55, 100))
    .setColorActive(color(0, 65, 120))
    .setColorLabel(color(0))
    .setColorValue(color(255))
    .setGroup(gridGroup)
    ;
    
  // Move label BELOW the slider (changed from top)
  Slider angleSlider = (Slider)cp5.getController("gridLinearAngle");
  angleSlider.getCaptionLabel()
           .align(ControlP5.LEFT, ControlP5.BOTTOM_OUTSIDE)
           .setPaddingY(5);
  
  // Speed slider - style matching screenshot
  cp5.addSlider("gridSpeed")
    .setPosition(controlHalfWidth + 40, wheelSize + 85)
    .setSize(controlHalfWidth, 20)
    .setRange(0.1, 10)
    .setValue(1)
    .setLabel("SPEED")
    .setColorBackground(color(0, 45, 90))
    .setColorForeground(color(0, 55, 100))
    .setColorActive(color(0, 65, 120))
    .setColorLabel(color(0))
    .setColorValue(color(255))
    .setGroup(gridGroup)
    ;
    
  // Move label BELOW the slider (changed from top)
  Slider speedSlider = (Slider)cp5.getController("gridSpeed");
  speedSlider.getCaptionLabel()
           .align(ControlP5.LEFT, ControlP5.BOTTOM_OUTSIDE)
           .setPaddingY(5);
  
  // Playback mode dropdown - style matching screenshot
  cp5.addScrollableList("gridPlayback")
    .setPosition(controlHalfWidth + 40, wheelSize + 125)
    .setSize(controlHalfWidth, 100)
    .setBarHeight(20)
    .setItemHeight(20)
    .addItems(java.util.Arrays.asList("loop", "boomerang", "once"))
    .setValue(0) // Default to loop
    .setLabel("PLAYBACK MODE")
    .setColorBackground(color(0, 45, 90))
    .setColorForeground(color(0, 55, 100))
    .setColorActive(color(0, 65, 120))
    .setGroup(gridGroup)
    ;
}

void setupStripControlGroup(float yPos) {
  // Adjusted width to fit within narrower window with 5px margin
  int groupWidth = (windowWidth - 40) / 2 - 10;
  
  // Create a group for strip controls - matching screenshot style
  Group stripGroup = cp5.addGroup("Strip Controls")
    .setPosition(windowWidth/2, yPos)
    .setWidth(groupWidth) 
    .setBackgroundHeight(400)
    .setBackgroundColor(color(240))
    .setBarHeight(25)
    .setColorBackground(color(100, 60, 100))
    .setColorLabel(color(255))
    .setColorActive(color(255, 128, 0))
    .disableCollapse()
    ;
    
  // Keep original wheel size
  int wheelSize = int(groupWidth * 0.4);
  
  // Original positions, just closer to top
  cp5.addColorWheel("stripStartColorWheel")
    .setPosition(30, 35)
    .setSize(wheelSize, wheelSize)
    .setRGB(stripStartColor)
    .setLabel("START COLOR")
    .setGroup(stripGroup)
    ;
    
  // End color wheel
  cp5.addColorWheel("stripEndColorWheel")
    .setPosition(wheelSize + 70, 35)
    .setSize(wheelSize, wheelSize)
    .setRGB(stripEndColor)
    .setLabel("END COLOR")
    .setGroup(stripGroup)
    ;
  
  int controlWidth = groupWidth - 40;
  int controlHalfWidth = (controlWidth - 20) / 2;
  
  // Effect dropdown - style matching screenshot
  cp5.addScrollableList("stripEffect")
    .setPosition(20, wheelSize + 45)
    .setSize(controlHalfWidth, 140)
    .setBarHeight(20)
    .setItemHeight(20)
    .addItems(java.util.Arrays.asList("none", "solid", "pulse", "linear", "constrainbow", "rainbow", "wipe"))
    .setValue(2) // Default to pulse (index 2)
    .setLabel("EFFECT")
    .setColorBackground(color(0, 45, 90))
    .setColorForeground(color(0, 55, 100))
    .setColorActive(color(0, 65, 120))
    .setGroup(stripGroup)
    ;
  
  // LED count input - style matching screenshot
  cp5.addNumberbox("stripCount")
    .setPosition(controlHalfWidth + 40, wheelSize + 45)
    .setSize(controlHalfWidth, 20)
    .setRange(1, 300)
    .setValue(stripCount)
    .setMultiplier(1)
    .setDirection(Controller.HORIZONTAL)
    .setLabel("NUMBER OF LEDS")
    .setColorBackground(color(0, 45, 90))
    .setColorForeground(color(0, 55, 100))
    .setColorActive(color(0, 65, 120))
    .setColorLabel(color(0))
    .setColorValue(color(255))
    .setGroup(stripGroup)
    ;
    
  // Move label BELOW the control (changed from top)
  Numberbox countBox = (Numberbox)cp5.getController("stripCount");
  countBox.getCaptionLabel()
        .align(ControlP5.LEFT, ControlP5.BOTTOM_OUTSIDE)
        .setPaddingY(5);
  
  // Speed slider - style matching screenshot
  cp5.addSlider("stripSpeed")
    .setPosition(controlHalfWidth + 40, wheelSize + 85)
    .setSize(controlHalfWidth, 20)
    .setRange(0.1, 10)
    .setValue(1.0)
    .setLabel("SPEED")
    .setColorBackground(color(0, 45, 90))
    .setColorForeground(color(0, 55, 100))
    .setColorActive(color(0, 65, 120))
    .setColorLabel(color(0))
    .setColorValue(color(255))
    .setGroup(stripGroup)
    ;
    
  // Move label BELOW the slider (changed from top)  
  Slider stripSpeedSlider = (Slider)cp5.getController("stripSpeed");
  stripSpeedSlider.getCaptionLabel()
                .align(ControlP5.LEFT, ControlP5.BOTTOM_OUTSIDE)
                .setPaddingY(5);
  
  // Playback mode dropdown - style matching screenshot
  cp5.addScrollableList("stripPlayback")
    .setPosition(controlHalfWidth + 40, wheelSize + 125)
    .setSize(controlHalfWidth, 100)
    .setBarHeight(20)
    .setItemHeight(20)
    .addItems(java.util.Arrays.asList("loop", "boomerang", "once"))
    .setValue(0) // Default to loop
    .setLabel("PLAYBACK MODE")
    .setColorBackground(color(0, 45, 90))
    .setColorForeground(color(0, 55, 100))
    .setColorActive(color(0, 65, 120))
    .setGroup(stripGroup)
    ;
}

// Set up Debug group at the bottom - now collapsible and collapsed by default
void setupDebugGroup(float yPos) {
  // Create a group for Debug - matching screenshot style, now collapsible
  Group debugGroup = cp5.addGroup("Debug")
    .setPosition((windowWidth - 400) / 2, yPos)
    .setWidth(400)
    .setBackgroundHeight(80)
    .setBackgroundColor(color(240))
    .setBarHeight(25)
    .setColorBackground(color(80, 80, 120))
    .setColorLabel(color(255))
    .setColorActive(color(255, 128, 0))
    .enableCollapse() // Make it collapsible
    .close() // Collapsed by default
    ;
  
  // FPS control slider - style matching screenshot
  cp5.addSlider("targetFPS")
     .setPosition(20, 10)
     .setSize(350, 20)
     .setRange(1, 60)
     .setValue(targetFPS)
     .setNumberOfTickMarks(12)
     .setLabel("TARGET FPS")
     .setColorBackground(color(0, 45, 90))
     .setColorForeground(color(0, 55, 100))
     .setColorActive(color(0, 65, 120))
     .setColorLabel(color(0))
     .setColorValue(color(255))
     .setGroup(debugGroup)
     ;
     
  // Left-align the label BELOW the slider (changed from top)
  Slider fpsSlider = (Slider)cp5.getController("targetFPS");
  fpsSlider.getCaptionLabel()
          .align(ControlP5.LEFT, ControlP5.BOTTOM_OUTSIDE)
          .setPaddingY(5);
}

// Utility function to get the host IP address
// Improved to work on both Windows and Mac OS
String getHostIP() {
  try {
    // First try the getLocalHost approach (works well on Windows)
    String ip = java.net.InetAddress.getLocalHost().getHostAddress();
    
    // If we got the loopback address (127.0.0.1 or similar), try an alternative method
    if (ip.startsWith("127.")) {
      // Try to find a non-loopback IPv4 address (works better on Mac OS)
      java.util.Enumeration<java.net.NetworkInterface> interfaces = java.net.NetworkInterface.getNetworkInterfaces();
      while (interfaces.hasMoreElements()) {
        java.net.NetworkInterface networkInterface = interfaces.nextElement();
        // Skip loopback and inactive interfaces
        if (networkInterface.isLoopback() || !networkInterface.isUp()) {
          continue;
        }
        
        // Look through all IP addresses assigned to this interface
        java.util.Enumeration<java.net.InetAddress> addresses = networkInterface.getInetAddresses();
        while (addresses.hasMoreElements()) {
          java.net.InetAddress addr = addresses.nextElement();
          // Only consider IPv4 addresses that aren't loopback
          if (!addr.isLoopbackAddress() && addr instanceof java.net.Inet4Address) {
            return addr.getHostAddress();
          }
        }
      }
    }
    return ip; // Return the original IP if we couldn't find a better one
  } catch (Exception e) {
    println("Error getting IP address: " + e.getMessage());
    return "Unknown";
  }
}
