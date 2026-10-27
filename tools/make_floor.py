"""
Generates cd/FLOOR.TGA: one grid cell of the floor, tiled endlessly by VDP2.

The Saturn's background hardware has no texture filtering, so every edge in the image is
a hard step on screen: blocky up close, flickering dots far away. This draws the grid line
with a solid core and in-between shades on each side ("baked" anti-aliasing), which the
hardware then shows as a soft edge.

The image is one tile; main.c scales it so FLOOR_TEXELS cover FLOOR_TILE_UNITS of world.
Keep the two in sync: run this, then rebuild.

Usage (from the project folder):  python tools/make_floor.py
"""
import struct

SIZE = 256        # texels per side: must match FLOOR_TEXELS in main.c (multiple of 8)
LINE_CORE = 10    # texels of solid line colour, centred on the tile edge (~5 world units)
EDGE_SHADES = 3   # blend steps on each side of the core

BACKGROUND = (26, 35, 68)   # RGB: dark navy, as before
LINE = (211, 204, 222)      # RGB: pale lavender, as before


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


# Palette: 0 = background, 1..EDGE_SHADES = blends, last = line
palette = [BACKGROUND]
palette += [lerp(BACKGROUND, LINE, (s + 1) / (EDGE_SHADES + 1)) for s in range(EDGE_SHADES)]
palette.append(LINE)
LINE_INDEX = len(palette) - 1


def shade(distance):
    """Palette index for a texel `distance` texels from the tile edge (the line's centre)."""
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


pixels = bytearray()
for y in range(SIZE):
    for x in range(SIZE):
        # A texel takes the brighter of its row line and column line, so crossings stay solid
        pixels.append(max(shade(edge_distance(x)), shade(edge_distance(y))))

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
print(f'cd/FLOOR.TGA: {SIZE}x{SIZE}, {len(palette)} colours, line core {LINE_CORE} + {EDGE_SHADES} shades each side')
