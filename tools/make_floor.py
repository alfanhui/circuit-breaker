"""
Generates cd/FLOOR.TGA: one tile of the floor, tiled endlessly by VDP2. It holds two pictures at
once, and the palette picks which one shows:

  - the arena's grid: one grid cell, the line with a solid core and in-between shades on each
    side ("baked" anti-aliasing: the Saturn's background hardware has no texture filtering, so
    every edge in the image is a hard step on screen)
  - the city's ground, a circuit board (src/game/board.h): traces, vias where they cross, pads.
    Seen on foot, from the roofs of Work RAM's city, drawn 32 times larger (view.c)

Each texel's palette index is  board_class x GRID_SHADES + grid_shade.
The palette in the file is the arena's: entry i is grid shade i % GRID_SHADES, whatever the board
class, so the arena sees only its grid. On foot main.c rewrites the palette with board.c's
colours, which go by i / GRID_SHADES instead: the board shows and the grid's gone. Swapping
pictures is 256 colour RAM writes, not a CD read and a new tile in VDP2's VRAM.
The traces' texels are one of BOARD_PHASES classes each, by how far along the trace they are:
lighting a few phases at a time and moving them on (palette cycling) sends pulses along the
traces for a handful of colour writes a frame.

The image is one tile; main.c scales it so FLOOR_TEXELS cover FLOOR_TILE_UNITS of world.
Keep the two (and src/game/board.h's classes) in sync: run this, then rebuild.

Usage (from the project folder):  python tools/make_floor.py
"""
import struct

SIZE = 256        # texels per side: must match FLOOR_TEXELS in main.c (multiple of 8)

# ---- The arena's grid (as before) ----
LINE_CORE = 10    # texels of solid line colour, centred on the tile edge (~5 world units)
EDGE_SHADES = 3   # blend steps on each side of the core
GRID_SHADES = EDGE_SHADES + 2  # background, the blends, the line: board.h's BOARD_GRID_SHADES

BACKGROUND = (26, 35, 68)   # RGB: dark navy, as before
LINE = (211, 204, 222)      # RGB: pale lavender, as before


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


# Grid shades: 0 = background, 1..EDGE_SHADES = blends, last = line
grid_colours = [BACKGROUND]
grid_colours += [lerp(BACKGROUND, LINE, (s + 1) / (EDGE_SHADES + 1)) for s in range(EDGE_SHADES)]
grid_colours.append(LINE)
LINE_INDEX = GRID_SHADES - 1


def shade(distance):
    """Grid shade for a texel `distance` texels from the tile edge (the line's centre)."""
    half_core = LINE_CORE // 2
    if distance < half_core:
        return LINE_INDEX
    step = distance - half_core   # 0 = right next to the core
    if step < EDGE_SHADES:
        return EDGE_SHADES - step  # brightest blend next to the core
    return 0


def edge_distance(i):
    # Distance from texel i's centre to the nearest tile edge (the edge is at 0 / SIZE)
    return min(i, SIZE - 1 - i)


# ---- The city's circuit board ----
# Board classes: as src/game/board.h has them
SUBSTRATE, TRACE_EDGE, PAD, HOLE = 0, 1, 2, 3
FIRST_PHASE = 4
PHASES = 32                   # BOARD_PHASES: along a trace, SIZE / PHASES = 8 texels each
CLASSES = FIRST_PHASE + PHASES
assert CLASSES * GRID_SHADES <= 256

# On foot a texel's ~15.6 world units (the tile's 4000 units across), seen from 2000-3000 up:
# anything much under 10 texels wide is a pixel or two and shimmers. So a bold board: wide
# traces, big vias
TRACE_HALF = 5    # the core: 11 texels wide (~170 units)
EDGE_HALF = 7     # and a darker rim, 2 texels each side
VIA_OUTER = 12    # a via's copper ring
VIA_HOLE = 5      # and its hole

# Horizontal traces, each one centre line across the whole tile and back to where it started
# (so the tile joins up), with 45 degree jogs: (x, y) corners. dir: which way its pulses run.
# off: where its pulses are, so they don't all move in step
HORIZONTALS = [
    dict(points=[(0, 22), (60, 22), (84, 46), (170, 46), (194, 22), (256, 22)], dir=1, off=0),
    dict(points=[(0, 92), (100, 92), (124, 116), (256, 116)], dir=-1, off=11),  # (jogs back: fix_wrap)
    dict(points=[(0, 160), (140, 160), (164, 184), (220, 184), (244, 160), (256, 160)], dir=1, off=5),
    dict(points=[(0, 222), (36, 222), (44, 230), (256, 230)], dir=-1, off=20),
]
# Straight vertical traces at these x, with their pulse direction and offset
VERTICALS = [dict(x=30, dir=1, off=3), dict(x=110, dir=-1, off=17), dict(x=210, dir=1, off=26)]
# Short stubs branching off, ending in a pad: (x0, y0) to (x1, y1), the pad at the second end
STUBS = [((150, 60), (150, 80)), ((70, 160), (70, 128)), ((180, 116), (180, 140))]


def fix_wrap(points):
    """Make a horizontal trace end at the height it started (the tile must join up)."""
    (x0, y0), (x1, y1) = points[0], points[-1]
    assert x0 == 0 and x1 == SIZE
    if y1 != y0:
        # Jog back to the start's height before the end, 45 degrees
        jog = abs(y1 - y0)
        points = points[:-1] + [(SIZE - jog - 4, y1), (SIZE - 4, y0), (SIZE, y0)]
    return points


for h in HORIZONTALS:
    h['points'] = fix_wrap(h['points'])


def segment_distance(px, py, ax, ay, bx, by):
    """Distance from (px, py) to the segment a-b, and how far along it (0..1)."""
    dx, dy = bx - ax, by - ay
    length2 = dx * dx + dy * dy
    t = 0.0 if length2 == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length2))
    cx, cy = ax + t * dx, ay + t * dy
    return ((px - cx) ** 2 + (py - cy) ** 2) ** 0.5


def wrapped(v):
    # The tile repeats: a texel near one edge is near what's drawn at the other
    return (v, v - SIZE, v + SIZE)


def near_polyline(px, py, points):
    best = 1e9
    for ox in (0, -SIZE, SIZE):
        for oy in (0, -SIZE, SIZE):
            for (ax, ay), (bx, by) in zip(points, points[1:]):
                best = min(best, segment_distance(px, py, ax + ox, ay + oy, bx + ox, by + oy))
    return best


def phase(along, direction, offset):
    """The phase class of a trace texel `along` texels from the tile's start (0..SIZE)."""
    step = (int(along) % SIZE) * PHASES // SIZE
    return FIRST_PHASE + ((step * direction + offset) % PHASES)


def board_class(x, y):
    px, py = x + 0.5, y + 0.5
    # Vias first: where each vertical crosses each horizontal (and the stubs' pads), on top
    pads = []
    for v in VERTICALS:
        for h in HORIZONTALS:
            pts = h['points']
            for (ax, ay), (bx, by) in zip(pts, pts[1:]):
                if ax <= v['x'] <= bx and bx != ax:
                    pads.append((v['x'], ay + (by - ay) * (v['x'] - ax) / (bx - ax)))
                    break
    pads += [end for _, end in STUBS]
    for cx, cy in pads:
        d = min(((px - X) ** 2 + (py - Y) ** 2) ** 0.5 for X in wrapped(cx) for Y in wrapped(cy))
        if d <= VIA_HOLE:
            return HOLE
        if d <= VIA_OUTER:
            return PAD
    # Then the traces: the nearest one's core or rim
    best, cls = 1e9, SUBSTRATE
    for h in HORIZONTALS:
        d = near_polyline(px, py, h['points'])
        if d < best:
            best, cls = d, phase(px, h['dir'], h['off'])
    for v in VERTICALS:
        d = min(abs(px - X) for X in wrapped(v['x']))
        if d < best:
            best, cls = d, phase(py, v['dir'], v['off'])
    for (ax, ay), (bx, by) in STUBS:
        d = near_polyline(px, py, [(ax, ay), (bx, by)])
        if d < best:
            best, cls = d, TRACE_EDGE if d > TRACE_HALF else PAD  # stubs don't pulse: solid copper
    if best <= TRACE_HALF:
        return cls
    if best <= EDGE_HALF:
        return TRACE_EDGE
    return SUBSTRATE


pixels = bytearray()
used = set()
for y in range(SIZE):
    for x in range(SIZE):
        # A texel's grid shade is the brighter of its row line and column line, so crossings stay solid
        g = max(shade(edge_distance(x)), shade(edge_distance(y)))
        b = board_class(x, y)
        used.add(b)
        pixels.append(b * GRID_SHADES + g)

# The arena's palette: whatever the board class, entry i is grid shade i % GRID_SHADES
palette = [grid_colours[i % GRID_SHADES] for i in range(256)]

header = struct.pack('<BBBHHBHHHHBB',
                     0,              # no image ID
                     1,              # has a colour map (palette)
                     1,              # uncompressed, colour-mapped
                     0, len(palette), 24,  # palette: first entry, count, bits per entry
                     0, 0, SIZE, SIZE,
                     8,              # bits per pixel
                     0)              # bottom-left origin, like the original
tga_palette = b''.join(bytes((b, g, r)) for (r, g, b) in palette)  # TGA stores BGR

with open('cd/FLOOR.TGA', 'wb') as f:
    f.write(header + tga_palette + pixels)
missing = [p for p in range(FIRST_PHASE, CLASSES) if p not in used]
print(f'cd/FLOOR.TGA: {SIZE}x{SIZE}, grid of {GRID_SHADES} shades, board of {CLASSES} classes'
      f' ({len(used)} used{", phases missing: " + str(missing) if missing else ""})')
