// ColorUtils.pde
// Utility functions for color manipulation

/**
 * Generates a random color between two colors
 * 
 * @param color1 First color
 * @param color2 Second color
 * @return Random color between color1 and color2
 */
color randomColorBetween(color color1, color color2) {
  // Generate random factor between 0 and 1
  float factor = random(1);
  
  // Interpolate between the colors using Processing's lerpColor
  return lerpColor(color1, color2, factor);
}

/**
 * Converts HSV values to RGB color
 * 
 * @param h Hue (0-1)
 * @param s Saturation (0-1)
 * @param v Value/Brightness (0-1)
 * @return RGB color
 */
color hsv2rgb(float h, float s, float v) {
  float r, g, b;
  
  // Implementation based on standard HSV to RGB algorithm
  int i = floor(h * 6);
  float f = h * 6 - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);
  
  // Calculate RGB values based on the sector of the HSV color wheel
  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    default: r = 0; g = 0; b = 0; break;
  }
  
  // Convert back to 0-255 range
  return color(r * 255, g * 255, b * 255);
}

/**
 * Converts RGB color to HSV values
 * 
 * @param c RGB color
 * @return float array [h, s, v] with values 0-1
 */
float[] rgb2hsv(color c) {
  float r = red(c) / 255.0;
  float g = green(c) / 255.0;
  float b = blue(c) / 255.0;
  
  float max = max(r, max(g, b));
  float min = min(r, min(g, b));
  float delta = max - min;
  
  float h = 0, s = 0, v = max;
  
  // Calculate saturation (0 if completely unsaturated)
  s = max == 0 ? 0 : delta / max;
  
  // Calculate hue
  if (delta == 0) {
    h = 0; // achromatic (gray)
  } else {
    // Calculate hue based on which RGB component is maximum
    if (max == r) {
      h = (g - b) / delta + (g < b ? 6 : 0);
    } else if (max == g) {
      h = (b - r) / delta + 2;
    } else { // max == b
      h = (r - g) / delta + 4;
    }
    h /= 6; // Normalize to 0-1 range
  }
  
  return new float[] {h, s, v};
}
