import ch.bildspur.artnet.*;

/*
 * ArtNetDevice — Sends LED data to a PrimusV2 receiver.
 *
 * Supports 3 output universes matching the hardware strip layout:
 *   Universe 0 → Strip A0 (Grid 8×4, 32 RGB LEDs)
 *   Universe 1 → Strip A1 (68 RGB LEDs)
 *   Universe 2 → Strip A2 (72 RGB LEDs)
 *
 * Each universe byte layout: [brightness][R][G][B]...
 */

class ArtNetDevice {
  ArtNetClient artnet;
  String deviceIP;
  boolean connected = false;

  int numOutputs;
  int[] ledCounts;
  byte[][] outputData;  // per-universe buffers

  byte brightnessValue = 50;

  ArtNetDevice(String deviceIP, int[] ledCounts) {
    this.deviceIP = deviceIP;
    this.numOutputs = ledCounts.length;
    this.ledCounts = ledCounts;

    // Allocate per-universe buffers: [brightness] + [RGB per pixel]
    outputData = new byte[numOutputs][];
    for (int i = 0; i < numOutputs; i++) {
      outputData[i] = new byte[1 + ledCounts[i] * 3];
      outputData[i][0] = brightnessValue;
    }

    try {
      artnet = new ArtNetClient(null);
      artnet.start();
      connected = true;
      println("ArtNet initialized → " + deviceIP + " [3 universes]");
    } catch (Exception e) {
      println("Error initializing ArtNet: " + e.getMessage());
      connected = false;
    }
  }

  void updateData(LED[][] gridLEDs, LED[] strip1LEDs, LED[] strip2LEDs, int brightness) {
    brightnessValue = (byte)brightness;

    // Universe 0: Grid (A0, 32 LEDs)
    outputData[0][0] = (byte)brightness;
    int idx = 1;
    for (int y = 0; y < gridLEDs.length; y++) {
      for (int x = 0; x < gridLEDs[y].length; x++) {
        color c = gridLEDs[y][x].currentColor;
        outputData[0][idx++] = (byte)((c >> 16) & 0xFF);
        outputData[0][idx++] = (byte)((c >> 8) & 0xFF);
        outputData[0][idx++] = (byte)(c & 0xFF);
      }
    }

    // Universe 1: Strip A1 (68 LEDs)
    outputData[1][0] = (byte)brightness;
    idx = 1;
    for (int i = 0; i < strip1LEDs.length; i++) {
      color c = strip1LEDs[i].currentColor;
      outputData[1][idx++] = (byte)((c >> 16) & 0xFF);
      outputData[1][idx++] = (byte)((c >> 8) & 0xFF);
      outputData[1][idx++] = (byte)(c & 0xFF);
    }

    // Universe 2: Strip A2 (72 LEDs)
    outputData[2][0] = (byte)brightness;
    idx = 1;
    for (int i = 0; i < strip2LEDs.length; i++) {
      color c = strip2LEDs[i].currentColor;
      outputData[2][idx++] = (byte)((c >> 16) & 0xFF);
      outputData[2][idx++] = (byte)((c >> 8) & 0xFF);
      outputData[2][idx++] = (byte)(c & 0xFF);
    }
  }

  void sendBlackout(int brightness) {
    if (!connected) return;
    try {
      for (int i = 0; i < numOutputs; i++) {
        byte[] blackout = new byte[outputData[i].length];
        blackout[0] = (byte)brightness;
        artnet.unicastDmx(deviceIP, i, 0, blackout);
      }
      delay(50);
      for (int i = 0; i < numOutputs; i++) {
        byte[] blackout = new byte[outputData[i].length];
        blackout[0] = (byte)brightness;
        artnet.unicastDmx(deviceIP, i, 0, blackout);
      }
      println("Sent blackout on universes 0, 1, 2");
    } catch (Exception e) {
      println("Error sending blackout: " + e.getMessage());
      connected = false;
    }
  }

  void sendData() {
    if (!connected) return;
    try {
      for (int i = 0; i < numOutputs; i++) {
        artnet.unicastDmx(deviceIP, i, 0, outputData[i]);
      }
    } catch (Exception e) {
      println("Error sending ArtNet data: " + e.getMessage());
      connected = false;
    }
  }

  void stop() {
    if (artnet != null) {
      artnet.stop();
    }
  }
}
