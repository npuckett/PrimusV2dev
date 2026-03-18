#!/usr/bin/env python3
"""
pulse_test.py — Simple ArtNet pulse sender for PrimusV2 receiver.

Sends a smooth pulsing color to each of the 3 strips:
  Universe 0 → Strip 0 (A0): 32 LEDs, red pulse
  Universe 1 → Strip 1 (A1): 68 LEDs, green pulse
  Universe 2 → Strip 2 (A2): 72 LEDs, blue pulse

Protocol per universe: [brightness] [R G B] [R G B] ...

Usage:
  pip install stupidartnet
  python pulse_test.py
  python pulse_test.py --ip 192.168.1.100 --brightness 80 --fps 30
"""

import argparse
import math
import time
import socket
import struct

# ── ArtNet Constants ─────────────────────────────────────────────────
ARTNET_HEADER = b"Art-Net\x00"
ARTNET_OPCODE_DMX = 0x5000  # little-endian in packet
ARTNET_VERSION = 14

# ── Strip Definitions ────────────────────────────────────────────────
STRIPS = [
    {"universe": 0, "count": 32, "color": (255, 0, 0),   "label": "A0 (32px, red)"},
    {"universe": 1, "count": 68, "color": (0, 255, 0),   "label": "A1 (68px, green)"},
    {"universe": 2, "count": 72, "color": (0, 0, 255),   "label": "A2 (72px, blue)"},
]


def build_artnet_dmx(universe: int, sequence: int, data: bytes) -> bytes:
    """Build a raw Art-Net DMX packet (ArtDmx, opcode 0x5000)."""
    length = len(data)
    # Header | Opcode (LE) | ProtVer (BE) | Sequence | Physical | Universe (LE) | Length (BE) | Data
    packet = bytearray()
    packet += ARTNET_HEADER
    packet += struct.pack("<H", ARTNET_OPCODE_DMX)
    packet += struct.pack(">H", ARTNET_VERSION)
    packet += struct.pack("B", sequence & 0xFF)
    packet += struct.pack("B", 0)  # physical port
    packet += struct.pack("<H", universe)
    packet += struct.pack(">H", length)
    packet += data
    return bytes(packet)


def make_pulse_data(strip: dict, brightness: int, t: float) -> bytes:
    """Generate DMX data for one strip: [brightness] [RGB * count].

    Pulse is a sine wave from 0 → full color → 0.
    """
    # Sine pulse: 0..1
    pulse = (math.sin(t * 2.0) + 1.0) / 2.0

    r = int(strip["color"][0] * pulse)
    g = int(strip["color"][1] * pulse)
    b = int(strip["color"][2] * pulse)

    data = bytearray()
    data.append(brightness & 0xFF)
    for _ in range(strip["count"]):
        data.append(r)
        data.append(g)
        data.append(b)
    return bytes(data)


def main():
    parser = argparse.ArgumentParser(description="PrimusV2 ArtNet pulse test sender")
    parser.add_argument("--ip", default="192.168.1.100", help="Receiver IP address")
    parser.add_argument("--brightness", type=int, default=80, help="LED brightness 0-255")
    parser.add_argument("--fps", type=int, default=30, help="Target frames per second")
    args = parser.parse_args()

    port = 6454  # Standard ArtNet UDP port
    interval = 1.0 / args.fps

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    print(f"PrimusV2 Pulse Test Sender")
    print(f"  Target:     {args.ip}:{port}")
    print(f"  Brightness: {args.brightness}")
    print(f"  FPS:        {args.fps}")
    print(f"  Strips:")
    for s in STRIPS:
        print(f"    Universe {s['universe']}: {s['label']}")
    print(f"\nSending... (Ctrl+C to stop)\n")

    seq = 0
    start = time.monotonic()

    try:
        while True:
            t = time.monotonic() - start

            for strip in STRIPS:
                data = make_pulse_data(strip, args.brightness, t)
                packet = build_artnet_dmx(strip["universe"], seq, data)
                sock.sendto(packet, (args.ip, port))

            seq = (seq + 1) & 0xFF

            # Print status every ~2 seconds
            if seq % (args.fps * 2) == 0:
                pulse_val = (math.sin(t * 2.0) + 1.0) / 2.0
                print(f"  t={t:.1f}s  pulse={pulse_val:.2f}  seq={seq}")

            time.sleep(interval)

    except KeyboardInterrupt:
        print("\nSending blackout...")
        for strip in STRIPS:
            black = bytes([args.brightness] + [0] * (strip["count"] * 3))
            packet = build_artnet_dmx(strip["universe"], seq, black)
            sock.sendto(packet, (args.ip, port))
            time.sleep(0.01)
            sock.sendto(packet, (args.ip, port))
        print("Done.")
    finally:
        sock.close()


if __name__ == "__main__":
    main()
