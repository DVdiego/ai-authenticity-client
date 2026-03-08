#!/usr/bin/env python3
from __future__ import annotations

import math
import pathlib
import struct
import subprocess
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ICONSET = ASSETS / "appcliente.iconset"
ICNS = ASSETS / "appcliente.icns"
PNG_256 = ASSETS / "appicon-256.png"


def clamp(value: float, low: float = 0.0, high: float = 1.0) -> float:
    return max(low, min(high, value))


def smoothstep(edge0: float, edge1: float, x: float) -> float:
    if edge0 == edge1:
        return 1.0 if x >= edge1 else 0.0
    t = clamp((x - edge0) / (edge1 - edge0))
    return t * t * (3.0 - 2.0 * t)


def mix(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def segment_distance(px: float, py: float, ax: float, ay: float, bx: float, by: float) -> float:
    vx = bx - ax
    vy = by - ay
    wx = px - ax
    wy = py - ay
    vv = vx * vx + vy * vy
    if vv == 0.0:
        return math.hypot(px - ax, py - ay)
    t = clamp((wx * vx + wy * vy) / vv)
    cx = ax + vx * t
    cy = ay + vy * t
    return math.hypot(px - cx, py - cy)


def rounded_rect_sdf(px: float, py: float, half_w: float, half_h: float, radius: float) -> float:
    qx = abs(px) - (half_w - radius)
    qy = abs(py) - (half_h - radius)
    ox = max(qx, 0.0)
    oy = max(qy, 0.0)
    outside = math.hypot(ox, oy)
    inside = min(max(qx, qy), 0.0)
    return outside + inside - radius


def composite(dst: tuple[float, float, float, float], src: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    dr, dg, db, da = dst
    sr, sg, sb, sa = src
    out_a = sa + da * (1.0 - sa)
    if out_a <= 0.0:
        return (0.0, 0.0, 0.0, 0.0)
    out_r = (sr * sa + dr * da * (1.0 - sa)) / out_a
    out_g = (sg * sa + dg * da * (1.0 - sa)) / out_a
    out_b = (sb * sa + db * da * (1.0 - sa)) / out_a
    return (out_r, out_g, out_b, out_a)


def write_png(path: pathlib.Path, width: int, height: int, pixels: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytearray()
    stride = width * 4
    for row in range(height):
        raw.append(0)
        start = row * stride
        raw.extend(pixels[start:start + stride])

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(chunk(b"IEND", b""))
    path.write_bytes(png)


def pixel_color(size: int, x: int, y: int) -> tuple[int, int, int, int]:
    px = ((x + 0.5) / size) * 2.0 - 1.0
    py = ((y + 0.5) / size) * 2.0 - 1.0
    aa = 2.0 / size

    top = (10 / 255, 24 / 255, 36 / 255)
    bottom = (7 / 255, 72 / 255, 88 / 255)
    white = (242 / 255, 247 / 255, 250 / 255)
    aqua = (108 / 255, 235 / 255, 224 / 255)
    amber = (242 / 255, 180 / 255, 72 / 255)
    navy = (7 / 255, 20 / 255, 29 / 255)

    layers: list[tuple[float, float, float, float]] = []

    bg_cov = 1.0 - smoothstep(0.0, aa * 1.8, rounded_rect_sdf(px, py, 0.92, 0.92, 0.24))
    if bg_cov > 0.0:
        grad = clamp((py + 1.0) * 0.5)
        layers.append((
            mix(top[0], bottom[0], grad),
            mix(top[1], bottom[1], grad),
            mix(top[2], bottom[2], grad),
            bg_cov,
        ))
        glow = clamp(1.0 - math.hypot(px + 0.24, py + 0.12) / 1.25)
        layers.append((aqua[0], aqua[1], aqua[2], 0.10 * glow * bg_cov))

    d = math.hypot(px, py)
    outer_ring = 1.0 - smoothstep(0.070, 0.070 + aa * 1.8, abs(d - 0.46))
    mid_ring = 1.0 - smoothstep(0.042, 0.042 + aa * 1.6, abs(d - 0.31))
    core = 1.0 - smoothstep(0.18, 0.18 + aa * 1.6, d)
    pupil = 1.0 - smoothstep(0.065, 0.065 + aa * 1.4, d)

    layers.append((white[0], white[1], white[2], 0.94 * outer_ring))
    layers.append((aqua[0], aqua[1], aqua[2], 0.82 * mid_ring))
    layers.append((aqua[0], aqua[1], aqua[2], 0.94 * core))
    layers.append((navy[0], navy[1], navy[2], 0.96 * pupil))

    target = 1.0 - smoothstep(0.055, 0.055 + aa * 1.3, math.hypot(px - 0.26, py + 0.26))
    pointer = 1.0 - smoothstep(0.015, 0.015 + aa * 1.2, segment_distance(px, py, 0.06, -0.06, 0.21, -0.21))
    layers.append((amber[0], amber[1], amber[2], 0.92 * target))
    layers.append((amber[0], amber[1], amber[2], 0.85 * pointer))

    color = (0.0, 0.0, 0.0, 0.0)
    for layer in layers:
        if layer[3] > 0.0:
            color = composite(color, layer)

    return (
        int(round(clamp(color[0]) * 255)),
        int(round(clamp(color[1]) * 255)),
        int(round(clamp(color[2]) * 255)),
        int(round(clamp(color[3]) * 255)),
    )


def render_png(path: pathlib.Path, size: int) -> None:
    data = bytearray()
    for y in range(size):
        for x in range(size):
            data.extend(pixel_color(size, x, y))
    write_png(path, size, size, bytes(data))


def main() -> None:
    ASSETS.mkdir(exist_ok=True)
    ICONSET.mkdir(exist_ok=True)

    sizes = {
        "icon_16x16.png": 16,
        "icon_16x16@2x.png": 32,
        "icon_32x32.png": 32,
        "icon_32x32@2x.png": 64,
        "icon_128x128.png": 128,
        "icon_128x128@2x.png": 256,
        "icon_256x256.png": 256,
        "icon_256x256@2x.png": 512,
        "icon_512x512.png": 512,
        "icon_512x512@2x.png": 1024,
    }
    for name, size in sizes.items():
        render_png(ICONSET / name, size)

    render_png(PNG_256, 256)
    subprocess.run(["/usr/bin/iconutil", "-c", "icns", str(ICONSET), "-o", str(ICNS)], check=True)
    print(f"Generated {ICNS}")


if __name__ == "__main__":
    main()
