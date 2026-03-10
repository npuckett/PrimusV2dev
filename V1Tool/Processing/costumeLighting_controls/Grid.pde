// Class to manage the 8x4 grid of LEDs

class Grid {
  int cols, rows;
  LED[][] leds;
  float cellSize;
  
  // Position of the grid on screen
  float xPos = 50;
  float yPos = 150; // Moved down further to provide more space below network controls
  float width, height;
  
  Grid(int cols, int rows) {
    this.cols = cols;
    this.rows = rows;
    
    // Keep original cell size
    cellSize = 70;
    width = cols * cellSize;
    height = rows * cellSize;
    
    // Position grid in the center horizontally
    xPos = (windowWidth - width) / 2; // Center horizontally in the window
    
    // Create grid of LEDs
    leds = new LED[rows][cols];
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        leds[y][x] = new LED(
          xPos + x * cellSize + cellSize/2, 
          yPos + y * cellSize + cellSize/2,
          cellSize * 0.8
        );
      }
    }
  }
  
  void display() {
    // Draw grid container
    fill(255);
    rect(xPos - 10, yPos - 10, width + 20, height + 20, 8);
    
    // Draw LEDs
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        leds[y][x].display();
      }
    }
  }
  
  void update() {
    // Calculate elapsed time in seconds adjusted by speed
    float elapsedSec = (millis() - startTime) / 1000.0 * gridSpeed;
    
    // Calculate animation progress factor based on playback mode
    float animFactor = 0;
    if (gridPlayback.equals("once")) {
      // For "once" mode, cap at 1.0 (100% complete)
      animFactor = min(elapsedSec * 0.2, 1);
    } else if (gridPlayback.equals("boomerang")) {
      // For "boomerang" mode, go from 0 to 1 and back to 0
      float cycle = (elapsedSec * 0.2) % 2;
      animFactor = cycle <= 1 ? cycle : 2 - cycle;
    } else {
      // For "loop" mode, cycle from 0 to 1 repeatedly
      animFactor = (elapsedSec * 0.2) % 1;
    }
    
    // Update each LED in the grid
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        updateLED(leds[y][x], x, y, animFactor, elapsedSec);
      }
    }
  }
  
  void updateLED(LED led, int x, int y, float animFactor, float elapsedSec) {
    // Apply different effects based on selected effect
    switch (gridEffect) {
      case "none":
        // Turn all LEDs off (black)
        led.setColor(color(0));
        break;
        
      case "solid":
        // Solid color using start color
        led.setColor(gridStartColor);
        break;
        
      case "pulse":
        // Pulse between start and end colors using sine wave
        float pulseFactor = (sin(elapsedSec) + 1) / 2; // 0 to 1
        
        // Interpolate between start and end colors
        color pulseColor = lerpColor(gridStartColor, gridEndColor, pulseFactor);
        led.setColor(pulseColor);
        break;
        
      case "linear":
        // Linear fade effect based on angle
        float angle = radians(gridLinearAngle);
        
        // Normalize position to -0.5 to 0.5 range
        float nx = x / float(cols - 1) - 0.5;
        float ny = y / float(rows - 1) - 0.5;
        
        // Create offset based on animation factor
        float offsetX = cos(animFactor * TWO_PI) * 0.2;
        float offsetY = sin(animFactor * TWO_PI) * 0.2;
        
        // Calculate dot product for direction
        PVector angleVector = new PVector(cos(angle), sin(angle));
        float dotProduct = (nx + offsetX) * angleVector.x + (ny + offsetY) * angleVector.y;
        
        // Map to 0-1 range
        float linearFactor = constrain(dotProduct + 0.5, 0, 1);
        
        // Interpolate between colors
        color linearColor = lerpColor(gridStartColor, gridEndColor, linearFactor);
        led.setColor(linearColor);
        break;
        
      case "constrainbow":
        // Update progress based on playback mode and speed
        if (gridPlayback.equals("once")) {
          // For "once" mode, cap progress at 1
          led.progress = min(led.progress + 0.01 * gridSpeed, 1);
        } else if (gridPlayback.equals("boomerang")) {
          // For "boomerang" mode, go back and forth
          float cyclePosition = (led.progress * 2) % 2;
          
          // Increment progress
          led.progress += 0.01 * gridSpeed;
          
          // Calculate interpolation factor based on cycle position
          float factor = cyclePosition < 1 ? cyclePosition : 2 - cyclePosition;
          
          // Interpolate RGB colors
          color interpColor = lerpColor(led.currentTargetColor, led.nextTargetColor, factor);
          led.setColor(interpColor);
          
          // If we've completed a full cycle, choose new colors
          if (led.progress >= 1) {
            color temp = led.currentTargetColor;
            led.currentTargetColor = led.nextTargetColor;
            led.nextTargetColor = randomColorBetween(gridStartColor, gridEndColor);
            led.progress = 0;
          }
          
          break;
        } else {
          // For "loop" mode, simply increment progress
          led.progress += 0.01 * gridSpeed;
        }
        
        // Check if we've reached the target
        if (led.progress >= 1) {
          // Choose new colors
          led.currentTargetColor = led.nextTargetColor;
          led.nextTargetColor = randomColorBetween(gridStartColor, gridEndColor);
          led.progress = 0;
        }
        
        // Interpolate RGB colors
        color constRainbowColor = lerpColor(led.currentTargetColor, led.nextTargetColor, led.progress);
        led.setColor(constRainbowColor);
        break;
        
      case "rainbow":
        // True rainbow effect: cycle through full RGB spectrum
        
        // Create an offset based on grid position for wave-like effect
        float rainbowOffset = (x / float(cols) + y / float(rows)) * 0.5;
        
        // Calculate base hue from elapsed time and position
        float rainbowHue = (elapsedSec * 0.2 + rainbowOffset) % 1;
        
        // Apply playback mode modifier
        if (gridPlayback.equals("once")) {
          // Cap at 1.0 for 'once' mode
          rainbowHue = min(rainbowHue, 1);
        } else if (gridPlayback.equals("boomerang")) {
          // Make it go forward and backward
          int cycle = floor(rainbowHue);
          rainbowHue = cycle % 2 == 0 ? rainbowHue % 1 : 1 - (rainbowHue % 1);
        }
        
        // Convert HSV to RGB (using full saturation and value)
        color rainbowColor = hsv2rgb(rainbowHue, 1, 1);
        led.setColor(rainbowColor);
        break;
        
      case "wipe":
        // Wipe effect: transition from start to end color horizontally
        
        // In boomerang mode, we need to handle the return transition differently
        if (gridPlayback.equals("boomerang")) {
          // For boomerang, use the full cycle position (0-2)
          float fullCycle = (elapsedSec * 0.2) % 2;
          
          // First half of cycle: 0->1 = start to end
          // Second half of cycle: 1->2 = end to start
          if (fullCycle <= 1) {
            // Forward transition (0->1)
            int wipeStep = ceil(fullCycle * cols);
            led.setColor(x < wipeStep ? gridEndColor : gridStartColor);
          } else {
            // Reverse transition (1->2)
            float reversePos = 2 - fullCycle; // Goes from 1->0
            int wipeStep = floor(reversePos * cols);
            led.setColor(x >= wipeStep ? gridStartColor : gridEndColor);
          }
        } else {
          // Standard wipe for loop and once modes
          int wipeStep = ceil(animFactor * cols);
          led.setColor(x < wipeStep ? gridEndColor : gridStartColor);
        }
        break;
    }
  }
  
  void resetColors() {
    // Reset constrainbow colors for each LED
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        leds[y][x].resetConstrainbowColors();
      }
    }
  }
}
