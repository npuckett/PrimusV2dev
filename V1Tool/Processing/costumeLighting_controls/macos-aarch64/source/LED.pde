
// Class to represent a single LED in the system

class LED {
  float x, y;
  float size;
  color currentColor = #888888; // Default gray
  
  // For constrainbow effect
  color currentTargetColor;
  color nextTargetColor;
  float progress = 0;
  
  LED(float x, float y, float size) {
    this.x = x;
    this.y = y;
    this.size = size;
    
    // Initialize random colors for constrainbow
    resetConstrainbowColors();
  }
  
  void display() {
    fill(currentColor);
    noStroke();
    ellipse(x, y, size, size);
  }
  
  void setColor(color c) {
    currentColor = c;
  }
  
  void resetConstrainbowColors() {
    // Reset colors used for constrainbow effect
    currentTargetColor = randomColorBetween(gridStartColor, gridEndColor);
    nextTargetColor = randomColorBetween(gridStartColor, gridEndColor);
    progress = 0;
  }
  
  // Returns the current LED color
  color getColor() {
    return currentColor;
  }
}
