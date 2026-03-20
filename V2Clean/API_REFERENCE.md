# PrimusV2 Art-Net API Reference

This document describes the network API exposed by PrimusV2 LED receiver nodes and strategies for integrating them into common creative-coding and lighting-control environments.

---

## Network Overview

| Function | Protocol | Port | Direction |
|---|---|---|---|
| **LED data** (ArtDmx) | UDP / Art-Net | 6454 | Sender → Node |
| **Discovery** (ArtPoll / ArtPollReply) | UDP / Art-Net | 6454 | Bidirectional |
| **FPS telemetry** (custom) | UDP | 6455 | Node → Sender |

All communication is standard Art-Net 4 over IPv4 UDP. No TCP, no HTTP, no proprietary framing — any software that speaks Art-Net can drive these nodes directly.

---

## 1. Discovery — ArtPoll / ArtPollReply

### How It Works

1. A controller broadcasts an **ArtPoll** packet (14 bytes) to port **6454**.
2. Every PrimusV2 node on the network replies with an **ArtPollReply** (239 bytes) containing its IP, name, port count, and universe mapping.
3. Nodes also broadcast an unsolicited ArtPollReply at startup, so controllers that are already listening will see them appear automatically.

### ArtPoll Packet (sender → network)

| Offset | Length | Field | Value |
|--------|--------|-------|-------|
| 0–7 | 8 | Header | `Art-Net\0` (ASCII + null) |
| 8–9 | 2 | Opcode | `0x2000` (little-endian) |
| 10–11 | 2 | ProtVer | `0x000E` (14, big-endian) |
| 12 | 1 | TalkToMe | `0x00` |
| 13 | 1 | Priority | `0x00` |

**Total: 14 bytes.** Send to `<broadcast>:6454` (255.255.255.255 or subnet broadcast like 192.168.1.255).

### ArtPollReply Packet (node → sender)

Key fields in the 239-byte reply:

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0–7 | 8 | Header | `Art-Net\0` |
| 8–9 | 2 | Opcode | `0x2100` (little-endian) |
| 10–13 | 4 | IP Address | Node's IPv4 address (4 bytes) |
| 14–15 | 2 | Port | `0x1936` (6454, little-endian) |
| 26–43 | 18 | Short Name | `"PrimusV2"` (null-terminated) |
| 44–107 | 64 | Long Name | e.g. `"PrimusV2 LED Node \| A0:Short Strip A1:Long Strip A2:Long Strip"` |
| 108–171 | 64 | Node Report | Status string, e.g. `"#0001 [0482] PrimusV2 OK — 30 fps"` |
| 172–173 | 2 | NumPorts | Number of active outputs (big-endian) |
| 174–177 | 4 | PortTypes | `0xC0` per active port (DMX output) |
| 190–193 | 4 | SwOut | Universe assignment per port (low nibble) |
| 201–206 | 6 | MAC Address | Node's WiFi MAC |

### Discovery Tips

- **Broadcast or unicast both work.** If broadcast is unreliable on your network, send ArtPoll directly to the node's IP.
- **Timeout of 2 seconds** is usually sufficient for WiFi nodes.
- Nodes re-broadcast their ArtPollReply at power-on, so persistent listeners will see new nodes appear without polling.

---

## 2. LED Data — ArtDmx

### Universe Layout

Each physical output maps to one Art-Net universe. The default configuration:

| Output | Type | Pixels | Bytes (RGB×3) | Universe |
|--------|------|-------:|----:|---------|
| A0 | Short Strip | 30 | 90 | 0 |
| A1 | Long Strip | 72 | 216 | 1 |
| A2 | Long Strip | 72 | 216 | 2 |

All data fits within the 512-byte Art-Net universe limit. One ArtDmx packet per universe, per frame.

### ArtDmx Packet Structure

| Offset | Length | Field | Value |
|--------|--------|-------|-------|
| 0–7 | 8 | Header | `Art-Net\0` |
| 8–9 | 2 | Opcode | `0x5000` (little-endian) |
| 10–11 | 2 | ProtVer | `0x000E` (14, big-endian) |
| 12 | 1 | Sequence | 1–255 (incrementing), 0 = disable |
| 13 | 1 | Physical | `0x00` |
| 14–15 | 2 | Universe | Universe number (little-endian) |
| 16–17 | 2 | Length | Data length in bytes (big-endian) |
| 18+ | N | Data | RGB pixel data (R, G, B, R, G, B, …) |

### Pixel Data Format

- **Color order:** RGB (3 bytes per pixel)
- **Channel mapping:** Pixel 0 = bytes 0–2, Pixel 1 = bytes 3–5, etc.
- **Brightness:** Controlled entirely by the RGB values you send. There is no separate brightness channel — send the exact colors you want.
- **Padding:** Art-Net requires even-length data. If your byte count is odd, pad with one `0x00`.

### Frame Rate

- The node renders data as fast as it arrives. For smooth animation, **30 FPS** is a good default. The hardware supports up to ~60+ FPS depending on strip length.
- Packets for universes 0, 1, and 2 within a **5 ms window** are assembled into a single frame. If a universe is missing, the timeout fires and the frame renders with stale data for that output.
- The **sequence byte** (offset 12) should increment 1→255→1 with each frame. This lets the node detect and discard out-of-order packets.

---

## 3. FPS Telemetry Back-Channel (Optional)

When enabled, the node sends a small status packet to the sender's IP on port **6455** once per second.

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0–2 | 3 | Magic | `"PFP"` (ASCII) |
| 3–4 | 2 | FPS | Frames per second (uint16, little-endian) |
| 5–6 | 2 | Packet Rate | Packets per second (uint16, little-endian) |

**Total: 7 bytes.** This is a custom (non-Art-Net) packet. Listen on UDP port 6455 if you want real-time performance data from the node. This is optional — the node functions identically whether or not anything is listening.

---

## 4. Integration Strategies by Tool

### TouchDesigner

TouchDesigner has native Art-Net support via the **Art-Net CHOP Out**.

1. **Discovery:** TouchDesigner's Art-Net nodes auto-discover via ArtPoll. PrimusV2 nodes should appear in the network interface automatically.
2. **Sending data:**
   - Create a **CHOP → Art-Net Out** node.
   - Set the **Universe** to match the output you want to drive (0, 1, or 2).
   - Set the **Destination IP** to the node's IP (or use broadcast for all nodes).
   - Feed RGB channel data as CHOP samples: channel count = pixel count × 3.
3. **Multiple outputs:** Create one Art-Net Out per universe, or use a single node with multi-universe output if your version supports it.
4. **FPS monitoring:** Use a **UDP In DAT** on port 6455 to capture the telemetry packet. Parse the 7 bytes to extract FPS.

### MadMapper / MadLight

MadMapper supports Art-Net natively and uses ArtPoll for discovery.

1. **Discovery:** Go to **Preferences → Protocols → Art-Net**. PrimusV2 nodes should appear in the device list after a network scan.
2. **Patch:** Map your LED fixtures to universes 0, 1, 2 with the appropriate pixel counts (30, 72, 72).
3. **Output:** MadMapper handles Art-Net packet construction internally — just assign fixtures to universes and map content.

### Isadora

Isadora supports Art-Net output via the **Art-Net Output** actor.

1. **Setup:** Enable Art-Net output in **Preferences → Communications → Art-Net**.
2. **Sending data:**
   - Use the **Art-Net Output** actor.
   - Set Universe to 0, 1, or 2.
   - Connect pixel data (RGB values) to the DMX channel inputs.
3. **Multi-universe:** Use one Art-Net Output actor per universe.
4. **Discovery:** Isadora may auto-detect via ArtPoll, or enter the IP manually.

### Processing (Java)

Use the [ArtNet4j](https://github.com/cansik/artnet4j) library or manual UDP.

#### Using artnet4j

```java
import ch.bildspur.artnet.*;

ArtNetClient artnet = new ArtNetClient();
artnet.start();  // binds to port 6454

// Send to universe 0
byte[] dmx = new byte[90];  // 30 pixels × 3 bytes
// fill dmx[] with RGB data ...
artnet.unicastDmx("192.168.1.100", 0, 0, dmx);

// Discovery
artnet.getNodeDiscovery().start();
// discovered nodes appear via callbacks
```

#### Manual UDP

```java
import java.net.*;

DatagramSocket sock = new DatagramSocket();
byte[] header = "Art-Net\0".getBytes("US-ASCII");

// Build ArtDmx packet
byte[] pkt = new byte[18 + numPixels * 3];
System.arraycopy(header, 0, pkt, 0, 8);
pkt[8] = 0x00; pkt[9] = 0x50;   // opcode 0x5000 LE
pkt[10] = 0x00; pkt[11] = 14;   // protocol version 14 BE
pkt[12] = sequence;              // 1–255
pkt[13] = 0;                     // physical
pkt[14] = (byte)(universe & 0xFF);
pkt[15] = (byte)((universe >> 8) & 0xFF);
int len = numPixels * 3;
pkt[16] = (byte)((len >> 8) & 0xFF);
pkt[17] = (byte)(len & 0xFF);
// copy RGB data into pkt[18..]
System.arraycopy(rgbData, 0, pkt, 18, len);

InetAddress addr = InetAddress.getByName("192.168.1.100");
sock.send(new DatagramPacket(pkt, pkt.length, addr, 6454));
```

### Pure Data (Pd)

Use the **[netsend]** object for raw UDP or the **mrpeach/net** externals.

```
[loadbang]
|
[connect 192.168.1.100 6454(
|
[netsend -u -b]   <-- UDP, binary mode
```

1. Construct the Art-Net header + pixel data as a list of bytes.
2. Pack with `[list2bytes]` or similar before sending.
3. For each frame, build the 18-byte header + RGB data and send.

Alternatively, the **[artnet]** external (if available) wraps this:
```
[artnet_send 192.168.1.100 0]   <-- IP, universe
```

### Python (Direct / without the companion tool)

The companion `led_controller.py` is one reference implementation. For custom scripts:

```python
import socket, struct

ARTNET_HEADER = b"Art-Net\x00"
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_artdmx(ip, universe, rgb_bytes, sequence=1):
    if len(rgb_bytes) % 2 != 0:
        rgb_bytes += b'\x00'
    pkt = bytearray()
    pkt += ARTNET_HEADER
    pkt += struct.pack("<H", 0x5000)           # ArtDmx opcode
    pkt += struct.pack(">H", 14)               # Protocol version
    pkt += bytes([sequence, 0])                 # Sequence, Physical
    pkt += struct.pack("<H", universe)          # Universe (LE)
    pkt += struct.pack(">H", len(rgb_bytes))   # Length (BE)
    pkt += rgb_bytes
    sock.sendto(pkt, (ip, 6454))

# Example: 30-pixel red strip on universe 0
data = bytes([255, 0, 0] * 30)
send_artdmx("192.168.1.100", 0, data)
```

### openFrameworks / C++

Use [ofxArtNet](https://github.com/hiroyuki/ofxArtnet) or raw UDP sockets:

```cpp
ofxUDPManager udp;
udp.Create();
udp.Connect("192.168.1.100", 6454);
udp.SetNonBlocking(true);

// Build packet (same byte layout as above)
unsigned char pkt[18 + 90];  // 30 pixels
memcpy(pkt, "Art-Net\0", 8);
pkt[8] = 0x00; pkt[9] = 0x50;  // opcode
// ... fill header fields ...
// ... copy RGB data ...
udp.Send((char*)pkt, sizeof(pkt));
```

### Max/MSP

Use the **[sadam.udpSender]** or **[mxj net.udp.send]** objects:

1. Build the Art-Net packet as a list of integers (0–255).
2. Send to `192.168.1.100` port `6454`.
3. For discovery, send the 14-byte ArtPoll packet and listen with **[udpreceive 6454]**.

### Any Software with sACN/E1.31

PrimusV2 currently speaks **Art-Net only** (not sACN). If your software only supports sACN, you'll need a bridge application (e.g. OLA — Open Lighting Architecture) to translate sACN → Art-Net.

---

## 5. Quick-Start Checklist

1. **Verify network.** The sender and PrimusV2 node must be on the same subnet (default: 192.168.1.x/24, WiFi SSID: NETGEAR44).
2. **Discover.** Send an ArtPoll to broadcast:6454. You should receive an ArtPollReply within ~100 ms.
3. **Send data.** Build ArtDmx packets for universes 0, 1, 2 with the correct pixel counts. Send to the node's IP on port 6454.
4. **Verify.** Watch the node's TFT display — it shows current FPS, source IP, and per-output status.
5. **Optional telemetry.** Listen on UDP port 6455 for 7-byte FPS packets from the node.

---

## 6. Adding a New Device or Output Type

### On the Arduino side ([config.h](V2Clean/Arduino/primusV2_receiver/config.h))

Add a row to `OUTPUT_TYPE_TABLE[]` and a value to the `OutputType` enum:

```c
enum OutputType {
  OUTPUT_OFF = 0,
  OUTPUT_SHORT_STRIP = 1,
  OUTPUT_LONG_STRIP = 2,
  OUTPUT_GRID = 3,
  OUTPUT_RING = 4,          // ← new
};

const OutputTypeDef OUTPUT_TYPE_TABLE[] = {
  { "Off",          0, 0, LAYOUT_NONE,   0, 0 },
  { "Short Strip", 30, 3, LAYOUT_LINEAR, 0, 0 },
  { "Long Strip",  72, 3, LAYOUT_LINEAR, 0, 0 },
  { "Grid 8x4",   32, 3, LAYOUT_GRID,   8, 4 },
  { "Ring",        24, 3, LAYOUT_LINEAR, 0, 0 },  // ← new
};
```

### On the sender side ([led_controller.py](V2Clean/sender/led_controller.py))

Add a matching entry to `OUTPUT_TYPES`:

```python
OUTPUT_TYPES = {
    "short_strip": {"pixels": 30, "layout": "linear"},
    "long_strip":  {"pixels": 72, "layout": "linear"},
    "grid":        {"pixels": 32, "layout": "grid", "grid_size": [8, 4]},
    "ring":        {"pixels": 24, "layout": "linear"},  # ← new
}
```

Pixel counts and byte sizes propagate automatically from these tables — no other code changes needed.
