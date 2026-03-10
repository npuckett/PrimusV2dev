// Class to manage the LED strip

class Strip {
  int count;
  LED[] leds;
  float ledSize = 20; // Keep original LED size
  float spacing = 5;  // Keep original spacing
  
  // Position of the strip on screen
  float xPos = 50;
  float yPos; // Will be set dynamically based on grid position
  float width;
  
  Strip(int count) {
    this.count = count;
    
    // Set yPos based on grid position (to be below grid)
    yPos = ledGrid.yPos + ledGrid.height + 30;
    
    // Calculate total width needed for all LEDs
    float totalWidth = (ledSize + spacing) * count;
    
    // Calculate how many LEDs we can fit per row based on windowWidth
    int maxLedsPerRow = (int) Math.floor((windowWidth - 100) / (ledSize + spacing));
    
    // Position strip centered horizontally
    xPos = (windowWidth - Math.min(totalWidth, maxLedsPerRow * (ledSize + spacing))) / 2;
    
    // Create array of LEDs
    leds = new LED[count];
    for (int i = 0; i < count; i++) {
      // Calculate row and column for this LED
      int row = i / maxLedsPerRow;
      int col = i % maxLedsPerRow;
      
      leds[i] = new LED(
        xPos + col * (ledSize + spacing) + ledSize/2,
        yPos + row * (ledSize + spacing) + ledSize/2,
        ledSize
      );
    }
    
    // Update width for display purposes
    width = Math.min(totalWidth, maxLedsPerRow * (ledSize + spacing));
  }
  
  void display() {
    // Calculate how many rows we need
    int maxLedsPerRow = (int) Math.floor((windowWidth - 100) / (ledSize + spacing));
    int rowCount = (int) Math.ceil((float)count / maxLedsPerRow);
    
    // Draw strip container
    fill(255);
    
    // Container for the entire strip
    float containerHeight = rowCount * (ledSize + spacing) + 10;
    rect(xPos - 10, yPos - 10, width + 20, containerHeight, 8);
    
    // Draw LEDs
    for (int i = 0; i < count; i++) {
      leds[i].display();
    }
  }
  
  void update() {
    // Calculate elapsed time in seconds adjusted by speed
    float elapsedSec = (millis() - startTime) / 1000.0 * stripSpeed;
    
    // Calculate animation progress factor based on playback mode
    float animFactor = 0;
    if (stripPlayback.equals("once")) {
      // For "once" mode, cap at 1.0 (100% complete)
      animFactor = min(elapsedSec * 0.2, 1);
    } else if (stripPlayback.equals("boomerang")) {
      // For "boomerang" mode, go from 0 to 1 and back to 0
      float cycle = (elapsedSec * 0.2) % 2;
      animFactor = cycle <= 1 ? cycle : 2 - cycle;
    } else {
      // For "loop" mode, cycle from 0 to 1 repeatedly
      animFactor = (elapsedSec * 0.2) % 1;
    }
    
    // Update each LED in the strip
    for (int i = 0; i < count; i++) {
      updateLED(leds[i], i, animFactor, elapsedSec);
    }
  }
  
  void updateLED(LED led, int i, float animFactor, float elapsedSec) {
    // Calculate position from 0 to 1 along the strip
    float position = count > 1 ? i / float(count - 1) : 0.5;
    
    // Apply different effects based on selected effect
    switch (stripEffect) {
      case "none":
        // Turn all LEDs off (black)
        led.setColor(color(0));
        break;
        
      case "solid":
        // Solid color using start color
        led.setColor(stripStartColor);
        break;
        
      case "pulse":
        // Pulse between start and end colors using sine wave
        float pulseFactor = (sin(elapsedSec) + 1) / 2; // 0 to 1
        
        // Interpolate between start and end colors
        color pulseColor = lerpColor(stripStartColor, stripEndColor, pulseFactor);
        led.setColor(pulseColor);
        break;
        
      case "linear":
        // Simplified linear fade effect for strip (always horizontal)
        
        // Apply animation offset for movement
        float shiftedPosition = (position + animFactor) % 1;
        
        // Interpolate between colors
        color linearColor = lerpColor(stripStartColor, stripEndColor, shiftedPosition);
        led.setColor(linearColor);
        break;
        
      case "constrainbow":
        // Update progress based on playback mode and speed
        if (stripPlayback.equals("once")) {
          // For "once" mode, cap progress at 1
          led.progress = min(led.progress + 0.01 * stripSpeed, 1);
        } else if (stripPlayback.equals("boomerang")) {
          // For "boomerang" mode, go back and forth
          float cyclePosition = (led.progress * 2) % 2;
          
          // Increment progress
          led.progress += 0.01 * stripSpeed;
          
          // Calculate interpolation factor based on cycle position
          float factor = cyclePosition < 1 ? cyclePosition : 2 - cyclePosition;
          
          // Interpolate RGB colors
          color interpColor = lerpColor(led.currentTargetColor, led.nextTargetColor, factor);
          led.setColor(interpColor);
          
          // If we've completed a full cycle, choose new colors
          if (led.progress >= 1) {
            color temp = led.currentTargetColor;
            led.currentTargetColor = led.nextTargetColor;
            led.nextTargetColor = randomColorBetween(stripStartColor, stripEndColor);
            led.progress = 0;
          }
          
          break;
        } else {
          // For "loop" mode, simply increment progress
          led.progress += 0.01 * stripSpeed;
        }
        
        // Check if we've reached the target
        if (led.progress >= 1) {
          // Choose new colors
          led.currentTargetColor = led.nextTargetColor;
          led.nextTargetColor = randomColorBetween(stripStartColor, stripEndColor);
          led.progress = 0;
        }
        
        // Interpolate RGB colors
        color constRainbowColor = lerpColor(led.currentTargetColor, led.nextTargetColor, led.progress);
        led.setColor(constRainbowColor);
        break;
        
      case "rainbow":
        // True rainbow effect using full HSV spectrum
        // Calculate hue based on position and time
        float position_factor = i / float(count - 1); // Normalize to 0-1
        float hue = (position_factor + elapsedSec * stripSpeed * 0.1) % 1;
        
        // Convert hue to RGB (full saturation and value)
        color rainbowColor = hsv2rgb(hue, 1, 1);
        led.setColor(rainbowColor);
        break;
        
      case "wipe":
        // Wipe effect: transition from start to end color horizontally
        int wipeIndex = floor(animFactor * count);
        
        // Set color based on wipe position
        if (i < wipeIndex) {
          led.setColor(stripEndColor);
        } else {
          led.setColor(stripStartColor);
        }
        break;
    }
  }
  
  void resetColors() {
    // Reset constrainbow colors for each LED
    for (int i = 0; i < count; i++) {
      leds[i].resetConstrainbowColors();
    }
  }
}
