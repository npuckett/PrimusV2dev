#!/usr/bin/env python3
"""
led_controller.py - Web-based GUI LED controller for PrimusV2 LED system.

Controls NeoPixel strips over WiFi using either raw UDP or Art-Net.
Each strip has independent effect, color, speed, and playback controls.
Supports multiple devices.

Runs a local web server and opens the control interface in your browser.

Edit the DEVICES list and PROTOCOL variable below to configure your setup.

Transport options (set PROTOCOL below):
    "udp"    — PrimusV2 raw UDP: single packet, lowest latency (default)
    "artnet" — Art-Net ArtDmx:  standard format, all strips in one packet

Usage:
    python3 led_controller.py
    python3 led_controller.py --port 8080

No external dependencies - uses only Python standard library.
"""

import argparse
import json
import math
import random
import socket
import struct
import threading
import time
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler


# ======================================================================
#  CONFIGURATION - edit this section to add devices / change strips
# ======================================================================

DEVICES = [
    {
        "name": "Costume 1",
        "ip": "192.168.1.100",
        "strips": [
            {"name": "A0", "universe": 0, "count": 32},
            {"name": "A1", "universe": 1, "count": 68},
            {"name": "A2", "universe": 2, "count": 72},
        ],
    },
    # To add another device, copy the block above and edit:
    # {
    #     "name": "Costume 2",
    #     "ip": "192.168.1.101",
    #     "strips": [
    #         {"name": "A0", "universe": 0, "count": 32, "grid": [8, 4]},
    #         {"name": "A1", "universe": 1, "count": 68},
    #     ],
    # },
]

DEFAULT_FPS = 30
DEFAULT_EFFECT = "pulse"
DEFAULT_SPEED = 1.0
DEFAULT_PLAYBACK = "loop"
DEFAULT_START_COLOR = [255, 0, 255]
DEFAULT_END_COLOR = [0, 255, 255]

EFFECT_NAMES = ["none", "solid", "pulse", "linear",
                "constrainbow", "rainbow", "wipe"]
PLAYBACK_MODES = ["loop", "boomerang", "once"]

# Transport protocol.  Change to "artnet" to use standard Art-Net format.
#   "udp"    — PrimusV2 raw UDP (fastest; custom magic "PV2" header)
#   "artnet" — Art-Net ArtDmx  (standard; all strips in one universe-0 packet)
PROTOCOL = "udp"


# ======================================================================
#  RAW UDP TRANSPORT — single packet per frame, all strips combined
# ======================================================================

# Packet format: "PV2" (3 bytes) + sequence (1 byte) + RGB data
# RGB data is all strips concatenated: [R G B] × strip0_count, then strip1, etc.
# Receiver knows strip sizes from its own config.
UDP_MAGIC = b"PV2"
UDP_PORT = 6454


class UDPSender:
    """Sends one combined UDP packet per frame to a device."""

    def __init__(self, ip):
        self.ip = ip
        self.sock = None
        self.connected = False
        self.sequence = 0

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.connected = True

    def disconnect(self):
        self.connected = False
        if self.sock:
            self.sock.close()
            self.sock = None

    def send_frame(self, rgb_data):
        """Send a single frame packet with all strips' RGB data."""
        if not self.connected or not self.sock:
            return
        pkt = bytearray(UDP_MAGIC)
        pkt.append(self.sequence & 0xFF)
        pkt += rgb_data
        self.sock.sendto(bytes(pkt), (self.ip, UDP_PORT))
        self.sequence = (self.sequence + 1) & 0xFF

    def blackout(self, total_pixels):
        if not self.connected:
            return
        self.send_frame(bytes(total_pixels * 3))


# ======================================================================
#  ART-NET TRANSPORT — standard Art-Net ArtDmx, single packet per frame
# ======================================================================

# Art-Net ArtDmx packet layout (Art-Net 4 spec):
#   ID[8]:        "Art-Net\0"
#   OpCode[2]:    0x00 0x50  (ArtDmx, little-endian)
#   ProtVerHi[1]: 0
#   ProtVerLo[1]: 14
#   Sequence[1]:  1-255 rolling counter (0 disables sequencing)
#   Physical[1]:  0
#   SubUni[1]:    universe low byte  (0)
#   Net[1]:       universe net byte  (0)
#   LengthHi[1]:  data length MSB
#   Length[1]:    data length LSB
#   Data[n]:      RGB pixel data for all strips concatenated
#
# All strips are packed into a single universe-0 packet.  The standard
# Art-Net limit is 512 bytes per universe; the PrimusV2 receiver accepts
# the slightly larger payload (up to MAX_UDP_PACKET bytes) needed to hold
# all LED data in one shot.

ARTNET_HEADER  = b"Art-Net\x00"
ARTNET_OPCODE_DMX = 0x5000   # ArtDmx opcode (sent little-endian)
ARTNET_VERSION = 14
ARTNET_PORT    = 6454


class ArtNetSender:
    """Sends one Art-Net ArtDmx packet per frame with all strips combined.

    All strip RGB data is concatenated into a single universe-0 packet,
    giving a standard Art-Net wire format that any Art-Net tool can see
    while still delivering the full frame in one UDP datagram.
    """

    def __init__(self, ip):
        self.ip = ip
        self.sock = None
        self.connected = False
        self.sequence = 1   # Art-Net spec: 1-255 (0 = sequencing disabled)

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.connected = True

    def disconnect(self):
        self.connected = False
        if self.sock:
            self.sock.close()
            self.sock = None

    def _build_packet(self, rgb_data):
        """Build an Art-Net ArtDmx packet containing all strip data."""
        # Art-Net spec requires even length; pad if necessary
        if len(rgb_data) % 2 != 0:
            rgb_data = rgb_data + b'\x00'
        length = len(rgb_data)
        pkt = bytearray()
        pkt += ARTNET_HEADER                               # ID[8]
        pkt += struct.pack("<H", ARTNET_OPCODE_DMX)        # OpCode (LE)
        pkt += struct.pack(">H", ARTNET_VERSION)           # ProtVer (BE)
        pkt += bytes([self.sequence])                      # Sequence
        pkt += bytes([0])                                  # Physical
        pkt += struct.pack("<H", 0)                        # Universe 0 (LE)
        pkt += struct.pack(">H", length)                   # Length (BE)
        pkt += rgb_data                                    # Data
        return bytes(pkt)

    def send_frame(self, rgb_data):
        """Send a single Art-Net frame with all strips' RGB data."""
        if not self.connected or not self.sock:
            return
        pkt = self._build_packet(rgb_data)
        self.sock.sendto(pkt, (self.ip, ARTNET_PORT))
        # Sequence counter: 1-255, wrapping back to 1 (never 0)
        self.sequence = (self.sequence % 255) + 1

    def blackout(self, total_pixels):
        if not self.connected:
            return
        self.send_frame(bytes(total_pixels * 3))


# ======================================================================
#  COLOR UTILITIES
# ======================================================================

def lerp_color(c1, c2, t):
    t = max(0.0, min(1.0, t))
    return (
        int(c1[0] + (c2[0] - c1[0]) * t),
        int(c1[1] + (c2[1] - c1[1]) * t),
        int(c1[2] + (c2[2] - c1[2]) * t),
    )


def hsv_to_rgb(h, s, v):
    i = int(h * 6.0)
    f = h * 6.0 - i
    p = v * (1.0 - s)
    q = v * (1.0 - f * s)
    t_ = v * (1.0 - (1.0 - f) * s)
    r, g, b = [
        (v, t_, p), (q, v, p), (p, v, t_),
        (p, q, v), (t_, p, v), (v, p, q),
    ][i % 6]
    return (int(r * 255), int(g * 255), int(b * 255))


def random_color_between(c1, c2):
    return lerp_color(c1, c2, random.random())


# ======================================================================
#  EFFECTS ENGINE
# ======================================================================

def compute_anim_factor(scaled_t, playback):
    if playback == "once":
        return min(scaled_t * 0.2, 1.0)
    elif playback == "boomerang":
        cyc = (scaled_t * 0.2) % 2.0
        return cyc if cyc <= 1.0 else 2.0 - cyc
    else:
        return (scaled_t * 0.2) % 1.0


def fx_none(count, **kw):
    return [(0, 0, 0)] * count


def fx_solid(count, start_color, **kw):
    return [start_color] * count


def fx_pulse(count, t, start_color, end_color, **kw):
    f = (math.sin(t) + 1.0) / 2.0
    c = lerp_color(start_color, end_color, f)
    return [c] * count


def fx_linear(count, anim_factor, start_color, end_color,
              grid=None, angle=0, **kw):
    pixels = []
    if grid:
        cols, rows = grid
        rad = math.radians(angle)
        ax, ay = math.cos(rad), math.sin(rad)
        ox = math.cos(anim_factor * math.tau) * 0.2
        oy = math.sin(anim_factor * math.tau) * 0.2
        for idx in range(count):
            x, y = idx % cols, idx // cols
            nx = x / max(cols - 1, 1) - 0.5
            ny = y / max(rows - 1, 1) - 0.5
            dot = (nx + ox) * ax + (ny + oy) * ay
            pixels.append(lerp_color(start_color, end_color,
                                     max(0.0, min(1.0, dot + 0.5))))
    else:
        for i in range(count):
            pos = i / max(count - 1, 1)
            shifted = (pos + anim_factor) % 1.0
            pixels.append(lerp_color(start_color, end_color, shifted))
    return pixels


def fx_constrainbow(count, dt, speed, playback,
                    start_color, end_color, state, **kw):
    while len(state) < count:
        state.append({
            "cur": random_color_between(start_color, end_color),
            "nxt": random_color_between(start_color, end_color),
            "prg": random.random(),
        })
    inc = dt * 0.3 * speed
    pixels = []
    for i in range(count):
        s = state[i]
        s["prg"] += inc
        if playback == "boomerang":
            cp = (s["prg"] * 2.0) % 2.0
            f = cp if cp < 1.0 else 2.0 - cp
            pixels.append(lerp_color(s["cur"], s["nxt"], f))
            if s["prg"] >= 1.0:
                s["cur"] = s["nxt"]
                s["nxt"] = random_color_between(start_color, end_color)
                s["prg"] = 0.0
        elif playback == "once":
            s["prg"] = min(s["prg"], 1.0)
            pixels.append(lerp_color(s["cur"], s["nxt"], s["prg"]))
        else:
            if s["prg"] >= 1.0:
                s["cur"] = s["nxt"]
                s["nxt"] = random_color_between(start_color, end_color)
                s["prg"] = 0.0
            pixels.append(lerp_color(s["cur"], s["nxt"], s["prg"]))
    return pixels


def fx_rainbow(count, t, grid=None, **kw):
    pixels = []
    if grid:
        cols, rows = grid
        for idx in range(count):
            x, y = idx % cols, idx // cols
            offset = (x / cols + y / rows) * 0.5
            hue = (t * 0.2 + offset) % 1.0
            pixels.append(hsv_to_rgb(hue, 1.0, 1.0))
    else:
        for i in range(count):
            pos = i / max(count - 1, 1)
            hue = (pos + t * 0.2) % 1.0
            pixels.append(hsv_to_rgb(hue, 1.0, 1.0))
    return pixels


def fx_wipe(count, t, anim_factor, start_color, end_color,
            grid=None, playback="loop", **kw):
    pixels = []
    if grid:
        cols, _rows = grid
        if playback == "boomerang":
            cyc = (t * 0.2) % 2.0
            if cyc <= 1.0:
                step = math.ceil(cyc * cols)
                for idx in range(count):
                    pixels.append(end_color if (idx % cols) < step
                                  else start_color)
            else:
                rev = 2.0 - cyc
                step = math.floor(rev * cols)
                for idx in range(count):
                    pixels.append(start_color if (idx % cols) >= step
                                  else end_color)
        else:
            step = math.ceil(anim_factor * cols)
            for idx in range(count):
                pixels.append(end_color if (idx % cols) < step
                              else start_color)
    else:
        wi = math.floor(anim_factor * count)
        for i in range(count):
            pixels.append(end_color if i < wi else start_color)
    return pixels


EFFECTS = {
    "none":         fx_none,
    "solid":        fx_solid,
    "pulse":        fx_pulse,
    "linear":       fx_linear,
    "constrainbow": fx_constrainbow,
    "rainbow":      fx_rainbow,
    "wipe":         fx_wipe,
}


# ======================================================================
#  CONTROLLER STATE  (thread-safe)
# ======================================================================

class ControllerState:
    """Holds all settings, computes animations, sends frames."""

    def __init__(self):
        self.lock = threading.Lock()
        self.running = True
        self.fps = DEFAULT_FPS
        self.start_time = time.monotonic()
        self.last_tick = self.start_time
        self.devices = []

        sender_cls = {"udp": UDPSender, "artnet": ArtNetSender}.get(PROTOCOL)
        if sender_cls is None:
            raise ValueError(
                f"Unknown PROTOCOL {PROTOCOL!r}. "
                "Valid options are 'udp' (raw PrimusV2) or 'artnet' (Art-Net ArtDmx)."
            )

        for dev_cfg in DEVICES:
            dev = {
                "name": dev_cfg["name"],
                "ip": dev_cfg["ip"],
                "connected": False,
                "sender": sender_cls(dev_cfg["ip"]),
                "strips": [],
            }
            for s_cfg in dev_cfg["strips"]:
                dev["strips"].append({
                    "name": s_cfg["name"],
                    "universe": s_cfg["universe"],
                    "count": s_cfg["count"],
                    "grid": s_cfg.get("grid"),
                    "effect": DEFAULT_EFFECT,
                    "start_color": list(DEFAULT_START_COLOR),
                    "end_color": list(DEFAULT_END_COLOR),
                    "speed": DEFAULT_SPEED,
                    "playback": DEFAULT_PLAYBACK,
                    "angle": 0,
                    "led_state": [],
                    "pixels": [],
                })
            self.devices.append(dev)

    def get_json(self):
        """Return full state as a JSON-serializable dict."""
        with self.lock:
            out = {"fps": self.fps, "devices": []}
            for dev in self.devices:
                d = {
                    "name": dev["name"],
                    "ip": dev["ip"],
                    "connected": dev["connected"],
                    "strips": [],
                }
                for s in dev["strips"]:
                    d["strips"].append({
                        "name": s["name"],
                        "count": s["count"],
                        "grid": s["grid"],
                        "universe": s["universe"],
                        "effect": s["effect"],
                        "start_color": s["start_color"],
                        "end_color": s["end_color"],
                        "speed": s["speed"],
                        "playback": s["playback"],
                        "angle": s["angle"],
                        "pixels": s["pixels"],
                    })
                out["devices"].append(d)
            return out

    def update(self, data):
        """Merge incoming settings from the browser."""
        with self.lock:
            if "fps" in data:
                self.fps = max(1, min(60, int(data["fps"])))
            di = data.get("device")
            if di is not None and 0 <= di < len(self.devices):
                dev = self.devices[di]
                if "ip" in data:
                    dev["ip"] = str(data["ip"])
                    dev["sender"].ip = dev["ip"]
                si = data.get("strip")
                if si is not None and 0 <= si < len(dev["strips"]):
                    s = dev["strips"][si]
                    if "effect" in data:
                        s["effect"] = str(data["effect"])
                        s["led_state"] = []
                    if "playback" in data:
                        s["playback"] = str(data["playback"])
                        s["led_state"] = []
                    if "speed" in data:
                        s["speed"] = max(0.1, min(10.0, float(data["speed"])))
                    if "angle" in data:
                        s["angle"] = float(data["angle"])
                    if "start_color" in data:
                        s["start_color"] = [int(v) for v in data["start_color"]]
                    if "end_color" in data:
                        s["end_color"] = [int(v) for v in data["end_color"]]

    def connect(self, di):
        with self.lock:
            dev = self.devices[di]
            dev["sender"].ip = dev["ip"]
            dev["sender"].connect()
            dev["connected"] = True

    def disconnect(self, di):
        with self.lock:
            dev = self.devices[di]
            if dev["sender"].connected:
                total = sum(s["count"] for s in dev["strips"])
                dev["sender"].blackout(total)
            dev["sender"].disconnect()
            dev["connected"] = False

    def tick(self):
        """Compute one animation frame and send a single UDP packet per device."""
        now = time.monotonic()
        send_queue = []  # [(sender, rgb_bytes), ...]

        with self.lock:
            t = now - self.start_time
            dt = max(now - self.last_tick, 0.001)
            self.last_tick = now

            for dev in self.devices:
                frame_buf = bytearray()
                for s in dev["strips"]:
                    speed = s["speed"]
                    scaled_t = t * speed
                    af = compute_anim_factor(scaled_t, s["playback"])
                    fn = EFFECTS.get(s["effect"], fx_none)
                    pixels = fn(
                        count=s["count"], t=scaled_t, dt=dt,
                        speed=speed, anim_factor=af,
                        playback=s["playback"],
                        start_color=tuple(s["start_color"]),
                        end_color=tuple(s["end_color"]),
                        state=s["led_state"],
                        grid=s.get("grid"), angle=s["angle"],
                    )
                    s["pixels"] = [list(p) for p in pixels]

                    if dev["sender"].connected:
                        for r, g, b in pixels:
                            frame_buf.extend((r & 0xFF, g & 0xFF, b & 0xFF))

                if frame_buf and dev["sender"].connected:
                    send_queue.append((dev["sender"], bytes(frame_buf)))

        # Send outside the lock
        for sender, data in send_queue:
            sender.send_frame(data)

    def shutdown(self):
        self.running = False
        for dev in self.devices:
            if dev["sender"].connected:
                total = sum(s["count"] for s in dev["strips"])
                dev["sender"].blackout(total)
                dev["sender"].disconnect()


# ======================================================================
#  ANIMATION THREAD
# ======================================================================

def animation_loop(state):
    """Fixed-rate animation loop that accounts for computation time."""
    next_frame = time.monotonic()
    while state.running:
        state.tick()
        next_frame += 1.0 / max(1, state.fps)
        sleep_time = next_frame - time.monotonic()
        if sleep_time > 0:
            time.sleep(sleep_time)
        else:
            # Fell behind — reset to avoid burst catch-up
            next_frame = time.monotonic()


# ======================================================================
#  HTML PAGE  (embedded - the entire browser UI)
# ======================================================================

HTML_PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PrimusV2 LED Controller</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  background:#111827;color:#e5e7eb;padding:16px 24px;
  max-width:1100px;margin:0 auto;
}
h1{font-size:22px;color:#818cf8;margin-bottom:16px}
.device{background:#1f2937;border-radius:10px;padding:16px;margin-bottom:16px;
  border:1px solid #374151}
.device h2{font-size:17px;color:#a5b4fc;margin-bottom:12px}
.net-row{display:flex;align-items:center;gap:8px;margin-bottom:12px;flex-wrap:wrap}
.net-row input[type=text]{background:#374151;border:1px solid #4b5563;color:#e5e7eb;
  border-radius:6px;padding:6px 10px;font-size:14px;width:155px}
.net-row input[type=text].connected{border-color:#34d399;box-shadow:0 0 6px #34d39944}
button{background:#4f46e5;color:#fff;border:none;border-radius:6px;padding:7px 14px;
  font-size:13px;cursor:pointer;font-weight:500}
button:hover{background:#6366f1}
button.danger{background:#b91c1c}
button.danger:hover{background:#dc2626}
.status-dot{width:10px;height:10px;border-radius:50%;background:#6b7280;
  display:inline-block}
.status-dot.on{background:#34d399}
.status-text{font-size:13px;color:#9ca3af}
canvas{display:block;margin:0 auto 12px;border-radius:6px;background:#0f172a}
.strips{display:flex;gap:10px;flex-wrap:wrap}
.strip-panel{flex:1;min-width:200px;background:#111827;border:1px solid #374151;
  border-radius:8px;padding:12px}
.strip-panel h3{font-size:14px;color:#c4b5fd;margin-bottom:10px}
.color-row{display:flex;gap:10px;margin-bottom:10px}
.color-box{display:flex;flex-direction:column;align-items:center;gap:3px}
.color-box label{font-size:11px;color:#9ca3af}
.color-box input[type=color]{width:52px;height:28px;border:none;border-radius:4px;
  cursor:pointer;background:none;padding:0}
.ctrl label{display:block;font-size:12px;color:#9ca3af;margin-bottom:3px}
.ctrl{margin-bottom:8px}
.ctrl select{width:100%;background:#374151;border:1px solid #4b5563;color:#e5e7eb;
  border-radius:5px;padding:5px 8px;font-size:13px}
.ctrl input[type=range]{width:100%}
.ctrl .val{font-size:12px;color:#e5e7eb;float:right}
.debug{background:#1f2937;border-radius:8px;padding:12px;border:1px solid #374151;
  display:flex;align-items:center;gap:10px;margin-top:16px}
.debug label{font-size:13px;color:#9ca3af;white-space:nowrap}
.debug input[type=range]{flex:1}
.debug .val{font-size:13px;color:#e5e7eb;width:28px;text-align:right}
</style>
</head>
<body>

<h1>PrimusV2 LED Controller</h1>
<div id="app">Loading...</div>

<script>
"use strict";

var S = null;
var EFFECTS = ["none","solid","pulse","linear","constrainbow","rainbow","wipe"];
var PLAYBACKS = ["loop","boomerang","once"];

function apiPost(path, body) {
  return fetch(path, {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(body)
  });
}

function fetchState() {
  return fetch("/api/state").then(function(r) { return r.json(); })
    .then(function(j) { S = j; });
}

function rgbHex(c) {
  return "#" + c.map(function(v) { return v.toString(16).padStart(2, "0"); }).join("");
}

function hexRgb(h) {
  return [parseInt(h.slice(1,3),16), parseInt(h.slice(3,5),16), parseInt(h.slice(5,7),16)];
}

function esc(s) {
  var d = document.createElement("div");
  d.textContent = s;
  return d.innerHTML;
}

function buildUI() {
  var app = document.getElementById("app");
  app.innerHTML = "";

  S.devices.forEach(function(dev, di) {
    var div = document.createElement("div");
    div.className = "device";
    div.id = "dev-" + di;

    var cclass = dev.connected ? " connected" : "";
    var dotclass = dev.connected ? " on" : "";
    var stxt = dev.connected ? "Connected" : "Disconnected";

    var html = '<h2>' + esc(dev.name) + '</h2>'
      + '<div class="net-row">'
      + '<label>IP:</label>'
      + '<input type="text" id="ip-' + di + '" value="' + esc(dev.ip) + '" class="' + cclass + '"'
      + ' onchange="apiPost(\\'/api/update\\',{device:' + di + ',ip:this.value})">'
      + '<button onclick="doConnect(' + di + ')">Connect</button>'
      + '<button class="danger" onclick="doDisconnect(' + di + ')">Disconnect</button>'
      + '<span class="status-dot' + dotclass + '" id="dot-' + di + '"></span>'
      + '<span class="status-text" id="stxt-' + di + '">' + stxt + '</span>'
      + '</div>'
      + '<canvas id="cv-' + di + '" height="10"></canvas>'

      + '<div class="strips" id="strips-' + di + '"></div>';
    div.innerHTML = html;
    app.appendChild(div);

    var sc = div.querySelector("#strips-" + di);
    dev.strips.forEach(function(st, si) {
      var p = document.createElement("div");
      p.className = "strip-panel";
      var gl = st.grid ? st.grid[0] + "x" + st.grid[1] : st.count + "px";
      var sh = rgbHex(st.start_color);
      var eh = rgbHex(st.end_color);

      var angleCtrl = "";
      if (st.grid) {
        angleCtrl = '<div class="ctrl">'
          + '<label>Angle: <span class="val" id="av-' + di + '-' + si + '">' + Math.round(st.angle) + '&deg;</span></label>'
          + '<input type="range" min="0" max="360" value="' + st.angle + '"'
          + ' oninput="document.getElementById(\\'av-' + di + '-' + si + '\\').innerHTML=Math.round(this.value)+\\'&deg;\\';'
          + 'apiPost(\\'/api/update\\',{device:' + di + ',strip:' + si + ',angle:+this.value})">'
          + '</div>';
      }

      var effectOpts = EFFECTS.map(function(e) {
        return "<option" + (e === st.effect ? " selected" : "") + ">" + e + "</option>";
      }).join("");
      var playOpts = PLAYBACKS.map(function(m) {
        return "<option" + (m === st.playback ? " selected" : "") + ">" + m + "</option>";
      }).join("");

      p.innerHTML = '<h3>' + esc(st.name) + ' &middot; ' + gl + '</h3>'
        + '<div class="color-row">'
        + '<div class="color-box"><label>Start</label>'
        + '<input type="color" value="' + sh + '"'
        + ' oninput="apiPost(\\'/api/update\\',{device:' + di + ',strip:' + si + ',start_color:hexRgb(this.value)})">'
        + '</div>'
        + '<div class="color-box"><label>End</label>'
        + '<input type="color" value="' + eh + '"'
        + ' oninput="apiPost(\\'/api/update\\',{device:' + di + ',strip:' + si + ',end_color:hexRgb(this.value)})">'
        + '</div></div>'
        + '<div class="ctrl"><label>Effect</label>'
        + '<select onchange="apiPost(\\'/api/update\\',{device:' + di + ',strip:' + si + ',effect:this.value})">'
        + effectOpts + '</select></div>'
        + '<div class="ctrl">'
        + '<label>Speed: <span class="val" id="sv-' + di + '-' + si + '">' + st.speed.toFixed(1) + '</span></label>'
        + '<input type="range" min="0.1" max="10" step="0.1" value="' + st.speed + '"'
        + ' oninput="document.getElementById(\\'sv-' + di + '-' + si + '\\').textContent=parseFloat(this.value).toFixed(1);'
        + 'apiPost(\\'/api/update\\',{device:' + di + ',strip:' + si + ',speed:+this.value})">'
        + '</div>'
        + '<div class="ctrl"><label>Playback</label>'
        + '<select onchange="apiPost(\\'/api/update\\',{device:' + di + ',strip:' + si + ',playback:this.value})">'
        + playOpts + '</select></div>'
        + angleCtrl;
      sc.appendChild(p);
    });
  });

  var dbg = document.createElement("div");
  dbg.className = "debug";
  dbg.innerHTML = '<label>FPS:</label>'
    + '<input type="range" min="1" max="60" value="' + S.fps + '"'
    + ' oninput="document.getElementById(\\'fv\\').textContent=this.value;'
    + 'apiPost(\\'/api/update\\',{fps:+this.value})">'
    + '<span class="val" id="fv">' + S.fps + '</span>';
  app.appendChild(dbg);

  sizeCanvases();
}

function sizeCanvases() {
  S.devices.forEach(function(dev, di) {
    var cv = document.getElementById("cv-" + di);
    if (!cv) return;
    cv.width = cv.parentElement.clientWidth - 34;
    var y = 8;
    dev.strips.forEach(function(st) {
      y += 16;
      if (st.grid) {
        y += st.grid[1] * 20 + 8;
      } else {
        var cs = 9;
        var maxPer = Math.floor((cv.width - 20) / cs);
        var nrows = Math.ceil(st.count / maxPer);
        y += nrows * cs + 8;
      }
    });
    cv.height = y + 8;
  });
}

function drawPreview() {
  if (!S) return;
  S.devices.forEach(function(dev, di) {
    var cv = document.getElementById("cv-" + di);
    if (!cv) return;
    var ctx = cv.getContext("2d");
    ctx.clearRect(0, 0, cv.width, cv.height);
    var y = 8;
    dev.strips.forEach(function(st) {
      ctx.fillStyle = "#6b7280";
      ctx.font = "11px sans-serif";
      ctx.fillText(st.name + " (" + st.count + "px)", 10, y + 10);
      y += 16;

      var px = st.pixels || [];
      if (st.grid) {
        var cols = st.grid[0], rows = st.grid[1], cs = 20;
        var gw = cols * cs, x0 = (cv.width - gw) / 2;
        for (var i = 0; i < px.length; i++) {
          var c = i % cols, r = Math.floor(i / cols);
          var rgb = px[i] || [0,0,0];
          ctx.fillStyle = "rgb(" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ")";
          ctx.beginPath();
          ctx.arc(x0 + c*cs + cs/2, y + r*cs + cs/2, cs/2 - 2, 0, Math.PI*2);
          ctx.fill();
        }
        y += rows * cs + 8;
      } else {
        var cs2 = 9;
        var maxPer = Math.floor((cv.width - 20) / cs2);
        for (var j = 0; j < px.length; j++) {
          var row = Math.floor(j / maxPer), col = j % maxPer;
          var rgb2 = px[j] || [0,0,0];
          ctx.fillStyle = "rgb(" + rgb2[0] + "," + rgb2[1] + "," + rgb2[2] + ")";
          ctx.fillRect(10 + col*cs2, y + row*cs2, cs2-1, cs2-1);
        }
        var nrows = Math.ceil(px.length / maxPer);
        y += nrows * cs2 + 8;
      }
    });
  });
}

function doConnect(di) {
  var ip = document.getElementById("ip-" + di).value;
  apiPost("/api/update", {device: di, ip: ip})
    .then(function() { return apiPost("/api/connect", {device: di}); })
    .then(function() { return fetchState(); })
    .then(function() { updateStatus(); });
}

function doDisconnect(di) {
  apiPost("/api/disconnect", {device: di})
    .then(function() { return fetchState(); })
    .then(function() { updateStatus(); });
}

function updateStatus() {
  S.devices.forEach(function(dev, di) {
    var dot = document.getElementById("dot-" + di);
    var txt = document.getElementById("stxt-" + di);
    var inp = document.getElementById("ip-" + di);
    if (dot) dot.className = "status-dot" + (dev.connected ? " on" : "");
    if (txt) txt.textContent = dev.connected ? "Connected" : "Disconnected";
    if (inp) {
      if (dev.connected) inp.classList.add("connected");
      else inp.classList.remove("connected");
    }
  });
}

function init() {
  fetchState().then(function() {
    buildUI();
    drawPreview();
    setInterval(function() {
      fetchState().then(function() {
        drawPreview();
        updateStatus();
      }).catch(function() {});
    }, 66);
    window.addEventListener("resize", function() { sizeCanvases(); drawPreview(); });
  });
}

init();
</script>
</body>
</html>"""


# ======================================================================
#  HTTP SERVER
# ======================================================================

class Handler(BaseHTTPRequestHandler):
    controller = None

    def do_GET(self):
        if self.path == "/":
            self._respond(200, "text/html", HTML_PAGE.encode())
        elif self.path == "/api/state":
            body = json.dumps(self.controller.get_json(),
                              separators=(",", ":")).encode()
            self._respond(200, "application/json", body)
        else:
            self._respond(404, "text/plain", b"Not Found")

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        data = json.loads(self.rfile.read(length)) if length else {}

        if self.path == "/api/update":
            self.controller.update(data)
        elif self.path == "/api/connect":
            di = data.get("device", 0)
            if 0 <= di < len(self.controller.devices):
                self.controller.connect(di)
        elif self.path == "/api/disconnect":
            di = data.get("device", 0)
            if 0 <= di < len(self.controller.devices):
                self.controller.disconnect(di)

        self._respond(200, "application/json", b'{"ok":true}')

    def _respond(self, code, ctype, body):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        pass


# ======================================================================
#  MAIN
# ======================================================================

def main():
    parser = argparse.ArgumentParser(
        description="PrimusV2 LED Controller - web-based GUI")
    parser.add_argument("--port", type=int, default=0,
                        help="HTTP port (0 = auto-select)")
    args = parser.parse_args()

    state = ControllerState()
    Handler.controller = state

    server = HTTPServer(("127.0.0.1", args.port), Handler)
    port = server.server_address[1]
    url = "http://127.0.0.1:{}".format(port)

    anim = threading.Thread(target=animation_loop, args=(state,), daemon=True)
    anim.start()

    print("PrimusV2 LED Controller")
    print("  URL:     {}".format(url))
    print("  Devices: {}".format(len(DEVICES)))
    for d in DEVICES:
        print("    {}: {} ({} strips)".format(
            d["name"], d["ip"], len(d["strips"])))
    print("")
    print("Opening browser... (Ctrl+C to stop)")
    print("")

    webbrowser.open(url)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("")
        print("Shutting down...")
    finally:
        state.shutdown()
        server.server_close()
        print("Done.")


if __name__ == "__main__":
    main()
