"""
Generates the broken glass over the hole you came through (src/level.c, the corridor's glass):
seen looking back from the corridor, the breach is a pane of dark glass shattered from where you
hit it. Cracks run out from the impact in rays, crossed by rings; between them the shards are
each a slightly different dark glass, and the cracks catch the light, pale cyan.

A real mirror would mean drawing the corridor twice (the Saturn has no reflections): this is the
cheap one. The pane's 1000 across (the widest hole: 4 wall pieces) and about 500 high: four
textures side by side, one per 250 piece, so the cracks run on across the joins.
  cd/GLASS0.TGA .. cd/GLASS3.TGA   64 x 128 each, 16 KB each in VDP1
Loaded in the hidden load at the breach (input.c), not at start-up.
Usage (from the project folder):  python tools/make_glass.py
"""
import math
import struct

PIECES = 4
WIDTH, HEIGHT = 64, 128          # texels per piece (VDP1: width a multiple of 8, height <= 255)
TOTAL = WIDTH * PIECES           # 256 across the pane: ~4 world units a texel both ways
IMPACT = (TOTAL * 0.5, HEIGHT * 0.82)  # where you hit it: the middle, at the cycle's height (row 0 = top)
RAYS = 11
RINGS = (12, 30, 58, 96)

SHARDS = [(10, 14, 30), (14, 20, 40), (8, 10, 24), (18, 24, 46), (12, 12, 34)]  # dark glass
CRACK = (200, 236, 255)          # the cracks catch the light
DUST = (230, 246, 255)           # crushed at the impact


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def ray_angle(i, r):
    """Ray i's angle at distance r: straight, but kinked a little at each ring it crosses, as
    cracks run: jagged, not curved"""
    kink = sum(1 for radius in RINGS if r > radius)
    base = (i + 0.35 * math.sin(i * 2.3)) * 2 * math.pi / RAYS
    return base + 0.06 * math.sin(i * 3.1 + kink * 2.2)


def texel(x, y):
    dx, dy = x - IMPACT[0], y - IMPACT[1]
    r = math.hypot(dx, dy)
    a = math.atan2(dy, dx) % (2 * math.pi)
    if r < 4:
        return DUST
    # Which two rays it's between (its shard's sector), and how near the nearest ray is
    angles = [ray_angle(i, r) % (2 * math.pi) for i in range(RAYS)]
    nearest, sector, lo, hi = 99.0, 0, 0.0, 0.0
    for i in range(RAYS):
        d = (a - angles[i] + math.pi) % (2 * math.pi) - math.pi
        nearest = min(nearest, abs(d) * r)
    order = sorted(range(RAYS), key=lambda i: angles[i])
    for k, i in enumerate(order):
        j = order[(k + 1) % RAYS]
        span = (angles[j] - angles[i]) % (2 * math.pi)
        if (a - angles[i]) % (2 * math.pi) <= span:
            sector, lo, hi = i, angles[i], angles[i] + span
            break
    # The rings: straight chords across each sector (a polygon round the impact), at radii that
    # differ a little from sector to sector
    mid, half = (lo + hi) / 2, (hi - lo) / 2
    along = r * math.cos(a - mid)  # how far out, square to the chord
    ring = 0
    for j, radius in enumerate(RINGS):
        chord = radius * (1 + 0.12 * math.sin(sector * 2.7 + j * 1.9)) * math.cos(half)
        # Not every sector's cracked round at every ring: the outer rings are broken in places
        if j >= 2 and (sector + j) % 3 == 0:
            continue
        nearest = min(nearest, abs(along - chord))
        if along > chord:
            ring = j + 1
    shard = SHARDS[(sector * 7 + ring * 3) % len(SHARDS)]
    # Glass: a little lighter towards the top, as if catching the sky, and a glint across some
    shard = lerp(shard, (40, 30, 70), max(0.0, 0.5 - y / HEIGHT) * 0.5)
    if (sector + ring) % 4 == 1 and abs((x - y * 0.6) % 37 - 18) < 1.5:
        shard = lerp(shard, CRACK, 0.25)
    width = 0.6 + 0.6 * max(0.0, 1 - r / 70)  # thicker near the impact
    if nearest < width:
        return CRACK
    if nearest < width + 1:
        return lerp(CRACK, shard, nearest - width)
    return shard


for p in range(PIECES):
    rows = [[texel(p * WIDTH + x + 0.5, y + 0.5) for x in range(WIDTH)] for y in range(HEIGHT)]
    # 24-bit, bottom row first, BGR (as tools/make_wall.py: jo's 8-bit sprites share one palette)
    pixels = bytearray(c for row in reversed(rows) for colour in row for c in reversed(colour))
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, WIDTH, HEIGHT, 24, 0)
    name = f'cd/GLASS{p}.TGA'
    with open(name, 'wb') as f:
        f.write(header + pixels)
    print(f'{name}: {WIDTH}x{HEIGHT}, {WIDTH * HEIGHT * 2 // 1024} KB in VDP1')
