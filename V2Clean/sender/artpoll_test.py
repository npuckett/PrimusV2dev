#!/usr/bin/env python3
"""Quick diagnostic: send ArtPoll and listen for ArtPollReply."""
import socket, struct, time

ARTNET_HEADER = b"Art-Net\x00"
ARTNET_PORT = 6454
DEVICE_IP = "192.168.1.100"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.settimeout(2.0)
sock.bind(("", ARTNET_PORT))

# Build ArtPoll (14 bytes)
poll = bytearray()
poll += ARTNET_HEADER
poll += struct.pack("<H", 0x2000)
poll += struct.pack(">H", 14)
poll += bytes([0x00, 0x00])

# --- Test 1: unicast to device ---
print(f"[1] Sending ArtPoll unicast to {DEVICE_IP}:{ARTNET_PORT}")
sock.sendto(bytes(poll), (DEVICE_IP, ARTNET_PORT))

deadline = time.monotonic() + 3.0
found = False
while time.monotonic() < deadline:
    try:
        raw, addr = sock.recvfrom(600)
        print(f"    Got {len(raw)} bytes from {addr}")
        if len(raw) >= 10 and raw[:8] == ARTNET_HEADER:
            opcode = struct.unpack("<H", raw[8:10])[0]
            print(f"    Opcode: 0x{opcode:04X}")
            if opcode == 0x2100:
                ip = f"{raw[10]}.{raw[11]}.{raw[12]}.{raw[13]}"
                name = raw[26:44].split(b"\x00")[0].decode("ascii", errors="replace")
                print(f"    => ArtPollReply: IP={ip}, name={name}")
                found = True
    except socket.timeout:
        break

if found:
    print("\nSUCCESS: Device responds to unicast ArtPoll.")
else:
    print("\nFAIL: No reply to unicast. Trying broadcast...")
    # --- Test 2: broadcast ---
    print(f"[2] Sending ArtPoll broadcast to 255.255.255.255:{ARTNET_PORT}")
    sock.sendto(bytes(poll), ("255.255.255.255", ARTNET_PORT))
    deadline2 = time.monotonic() + 3.0
    while time.monotonic() < deadline2:
        try:
            raw, addr = sock.recvfrom(600)
            print(f"    Got {len(raw)} bytes from {addr}")
        except socket.timeout:
            break
    print("FAIL: No reply to broadcast either.")

    # --- Test 3: subnet broadcast ---
    print(f"\n[3] Sending ArtPoll to subnet broadcast 192.168.1.255:{ARTNET_PORT}")
    sock.sendto(bytes(poll), ("192.168.1.255", ARTNET_PORT))
    deadline3 = time.monotonic() + 3.0
    while time.monotonic() < deadline3:
        try:
            raw, addr = sock.recvfrom(600)
            print(f"    Got {len(raw)} bytes from {addr}")
        except socket.timeout:
            break
    print("FAIL: No reply to subnet broadcast either.")

sock.close()
print("\nDone.")
