import ch.bildspur.artnet.*;

/*
 * ArtNetDevice — Sends LED data to a PrimusV2 (or V1) receiver.
 *
 * Supports two modes controlled by the 'useV2Protocol' flag:
 *   V1 (legacy):  All data packed into a single universe 0.
 *   V2 (new):     Grid → universe 0, Strip → universe 1.
 *                  Each universe: [brightness][R][G][B]...
 *
 * V2 mode matches the PrimusV2 receiver where each physical output
 * listens on its own ArtNet universe.
 */

class ArtNetDevice {
  ArtNetClient artnet;
  String deviceIP;
  boolean connected = false;
  boolean useV2Protocol = false; // false = V1 legacy, true = V2 multi-universe

  // V1 legacy: single combined buffer for universe 0
  byte[] combinedData;
  int v1Universe = 0;

  // V2: separate buffers per universe
  byte[] gridData;   // universe 0: [brightness][RGB...]
  byte[] stripData;  // universe 1: [brightness][RGB...]
  int gridUniverse  = 0;
  int stripUniverse = 1;

  byte brightnessValue = 50;

  // Track LED counts for blackout sizing
  int gridLEDCount;
  int stripLEDCount;

  ArtNetDevice(int universe, String deviceIP, int totalLEDs) {
    this(universe, deviceIP, totalLEDs, false);
  }

  ArtNetDevice(int universe, String deviceIP, int totalLEDs, boolean v2Mode) {
    this.deviceIP = deviceIP;
    this.v1Universe = universe;
    this.useV2Protocol = v2Mode;

    // Calculate grid count (always 32 = 8×4) and strip count
    this.gridLEDCount = 32;
    this.stripLEDCount = totalLEDs - gridLEDCount;

    // V1 legacy buffer
    combinedData = new byte[1 + totalLEDs * 3];
    combinedData[0] = brightnessValue;

    // V2 per-universe buffers
    gridData  = new byte[1 + gridLEDCount * 3];
    stripData = new byte[1 + max(stripLEDCount, 0) * 3];
    gridData[0]  = brightnessValue;
    stripData[0] = brightnessValue;

    try {
      artnet = new ArtNetClient(null);
      artnet.start();
      connected = true;
      println("ArtNet initialized → " + deviceIP + (v2Mode ? " [V2 multi-universe]" : " [V1 legacy]"));
    } catch (Exception e) {
      println("Error initializing ArtNet: " + e.getMessage());
      connected = false;
    }
  }

  void updateData(LED[][] gridLEDs, LED[] stripLEDs, int brightness) {
    brightnessValue = (byte)brightness;

    if (useV2Protocol) {
      updateDataV2(gridLEDs, stripLEDs, brightness);
    } else {
      updateDataV1(gridLEDs, stripLEDs, brightness);
    }
  }

  // V1 legacy: everything in one buffer, one universe
  private void updateDataV1(LED[][] gridLEDs, LED[] stripLEDs, int brightness) {
    combinedData[0] = (byte)brightness;
    int index = 1;

    for (int y = 0; y < gridLEDs.length; y++) {
      for (int x = 0; x < gridLEDs[y].length; x++) {
        color c = gridLEDs[y][x].currentColor;
        combinedData[index++] = (byte)((c >> 16) & 0xFF);
        combinedData[index++] = (byte)((c >> 8) & 0xFF);
        combinedData[index++] = (byte)(c & 0xFF);
      }
    }

    for (int i = 0; i < stripLEDs.length; i++) {
      color c = stripLEDs[i].currentColor;
      combinedData[index++] = (byte)((c >> 16) & 0xFF);
      combinedData[index++] = (byte)((c >> 8) & 0xFF);
      combinedData[index++] = (byte)(c & 0xFF);
    }
  }

  // V2: grid → universe 0, strip → universe 1
  private void updateDataV2(LED[][] gridLEDs, LED[] stripLEDs, int brightness) {
    // Grid buffer
    gridData[0] = (byte)brightness;
    int idx = 1;
    for (int y = 0; y < gridLEDs.length; y++) {
      for (int x = 0; x < gridLEDs[y].length; x++) {
        color c = gridLEDs[y][x].currentColor;
        gridData[idx++] = (byte)((c >> 16) & 0xFF);
        gridData[idx++] = (byte)((c >> 8) & 0xFF);
        gridData[idx++] = (byte)(c & 0xFF);
      }
    }

    // Strip buffer
    stripData[0] = (byte)brightness;
    idx = 1;
    for (int i = 0; i < stripLEDs.length; i++) {
      color c = stripLEDs[i].currentColor;
      stripData[idx++] = (byte)((c >> 16) & 0xFF);
      stripData[idx++] = (byte)((c >> 8) & 0xFF);
      stripData[idx++] = (byte)(c & 0xFF);
    }
  }

  void sendBlackout(int brightness) {
    if (!connected) return;
    try {
      if (useV2Protocol) {
        byte[] gridBlack  = new byte[1 + gridLEDCount * 3];
        byte[] stripBlack = new byte[1 + max(stripLEDCount, 0) * 3];
        gridBlack[0]  = (byte)brightness;
        stripBlack[0] = (byte)brightness;

        artnet.unicastDmx(deviceIP, gridUniverse, 0, gridBlack);
        artnet.unicastDmx(deviceIP, stripUniverse, 0, stripBlack);
        delay(50);
        artnet.unicastDmx(deviceIP, gridUniverse, 0, gridBlack);
        artnet.unicastDmx(deviceIP, stripUniverse, 0, stripBlack);
        println("Sent V2 blackout on universes " + gridUniverse + " & " + stripUniverse);
      } else {
        byte[] blackoutData = new byte[combinedData.length];
        blackoutData[0] = (byte)brightness;
        artnet.unicastDmx(deviceIP, v1Universe, 0, blackoutData);
        delay(50);
        artnet.unicastDmx(deviceIP, v1Universe, 0, blackoutData);
        println("Sent V1 blackout on universe " + v1Universe);
      }
    } catch (Exception e) {
      println("Error sending blackout: " + e.getMessage());
      connected = false;
    }
  }

  void sendData() {
    if (!connected) return;
    try {
      if (useV2Protocol) {
        artnet.unicastDmx(deviceIP, gridUniverse, 0, gridData);
        artnet.unicastDmx(deviceIP, stripUniverse, 0, stripData);
      } else {
        artnet.unicastDmx(deviceIP, v1Universe, 0, combinedData);
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
