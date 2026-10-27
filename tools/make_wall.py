"""
Generates the arena wall textures: the floor's grid carried up the wall, with a glowing base.

Why: a dark featureless wall reads as a hole in the world. Carrying the floor grid up the wall
(same 125-unit rows, vertical lines where floor lines meet the base) and a glowing line along
the base makes the wall visibly stand on the floor.

Each wall piece is two quads, so it can be drawn safely close to the camera (src/level.c):
  cd/WALL_LO.TGA  z = -8 .. 250:   the glowing base and first grid row
  cd/WALL_HI.TGA  z = 250 .. 1500: the rest, with a glowing top edge
  cd/WALLBASE.TGA  z = -8 .. 40:   just the glowing base, for iso cutaway walls
They're drawn from one design: the grid row at z = 250 lands on the join, half in each.

VDP1 maps a texture across a quad without perspective correction, so only lines along the
quad's edges, and straight lines between its edges, land exactly where the floor's would.
So vertical lines sit only on the piece's left and right edges (half on each, so two pieces
side by side make one full line); horizontal rows cross the whole piece.
VDP1 limits: width a multiple of 8, height at most 255.

Keep the numbers below in step with src/level.c (piece width, the three wall heights).
Usage (from the project folder):  python tools/make_wall.py
"""
import struct

WIDTH = 64                     # texels across one 250-unit piece
WALL_BOTTOM, WALL_SPLIT, WALL_TOP = -8, 250, 1500  # world z (level.c)
WALL_BASE_TOP = 40             # top of the cutaway base strip (level.c)
GRID_UNITS = 125               # the floor's tile size: rows every 125 up the wall

BODY = (16, 20, 44)            # a touch darker than the floor (26, 35, 68): walls recede
LINE = (211, 204, 222)         # the floor's line colour
GLOW = (255, 110, 180)         # base and top edge: the horizon haze pink, brighter


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


LINE_SHADES = [LINE, lerp(BODY, LINE, 2 / 3), lerp(BODY, LINE, 1 / 3)]               # core, then outwards
GLOW_SHADES = [GLOW, lerp(BODY, GLOW, 3 / 4), lerp(BODY, GLOW, 2 / 4), lerp(BODY, GLOW, 1 / 4)]


def band(distance, core, shades):
    """Colour at `distance` texels from a line's centre, or None outside it."""
    if distance < core:
        return shades[0]
    step = distance - core + 1
    return shades[step] if step < len(shades) else None


def render(z_bottom, z_top, height, filename):
    def texel_for_z(z):
        """Texel row (0 = top of the image) of world height z."""
        return round((z_top - z) * (height - 1) / (z_top - z_bottom))

    rows = []
    for y in range(height):
        row = []
        for x in range(WIDTH):
            colour = BODY
            # Horizontal grid rows every GRID_UNITS above the floor (z = 0 is the base glow instead)
            for z in range(GRID_UNITS, WALL_TOP, GRID_UNITS):
                if z_bottom <= z <= z_top:
                    hit = band(abs(y - texel_for_z(z)), 1, LINE_SHADES)
                    if hit is not None:
                        colour = hit
            # Vertical lines on the piece edges: half a line each side
            hit = band(min(x, WIDTH - 1 - x), 1, LINE_SHADES)
            if hit is not None:
                colour = hit
            # Glowing base (where the wall meets the floor) and top edge, drawn over the grid
            for z, core in ((0, 3), (WALL_TOP, 2)):
                if z_bottom <= z <= z_top:
                    hit = band(abs(y - texel_for_z(z)), core, GLOW_SHADES)
                    if hit is not None:
                        colour = hit
            row.append(colour)
        rows.append(row)

    # 24-bit colour, not 8-bit with a palette: jo's 8-bit VDP1 sprites all read the first
    # palette loaded (WALL1.TGA's), so an 8-bit wall comes out in the wrong colours.
    # TGA with a bottom-left origin stores the bottom row first; pixels are BGR
    pixels = bytearray(c for row in reversed(rows) for colour in row for c in reversed(colour))
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, WIDTH, height, 24, 0)
    with open(filename, 'wb') as f:
        f.write(header + pixels)
    print(f'{filename}: {WIDTH}x{height}, z {z_bottom}..{z_top}, {WIDTH * height * 2 // 1024} KB in VDP1')


# About 5-6 world units per texel row in both
render(WALL_BOTTOM, WALL_SPLIT, 48, 'cd/WALL_LO.TGA')
render(WALL_SPLIT, WALL_TOP, 208, 'cd/WALL_HI.TGA')
render(WALL_BOTTOM, WALL_BASE_TOP, 8, 'cd/WALLBASE.TGA')  # disc names are 8.3: 8 characters at most
