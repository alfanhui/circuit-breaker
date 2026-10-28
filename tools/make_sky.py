"""
Generates cd/SKY.TGA: the never-ending sunset above the arena walls.

The sky is the second half of the floor's VDP2 screen (RBG0 plane B, see main.c), a 128x8
image repeated sideways as the camera turns. The gradient runs along the image's width: texel
0 sits on the horizon and higher texels are higher up the screen. Every row is the same.

In the 1st person view one texel covers about 1.5 screen rows, and the top of the screen shows
about texel 77 (measured from a screenshot). The old image went black from texel 53, which
showed as a black band across the top of the sky. This one deepens to indigo by TOP_TEXEL and
holds it to the end, so the sunset never runs out, even if the camera looks higher.

Usage (from the project folder):  python tools/make_sky.py
"""
import struct

WIDTH = 128  # along the gradient: a multiple of 8 (VDP2 cells)
HEIGHT = 8
TOP_TEXEL = 84  # where the deepest colour is reached: a little past the top of the screen

# (texel, RGB): colours between stops are blended. Up to texel 40 these are the original
# image's colours, so the glow over the wall tops looks as before; above that it used to step
# to black at 53
STOPS = [
    (0, (211, 204, 222)),   # the pale line of the sun on the horizon
    (4, (251, 126, 182)),   # hot pink
    (6, (208, 104, 161)),
    (8, (183, 91, 150)),
    (12, (172, 85, 146)),
    (20, (149, 74, 137)),
    (28, (119, 59, 127)),   # purple
    (34, (90, 45, 119)),
    (38, (61, 32, 114)),
    (40, (30, 21, 110)),    # violet
    (60, (18, 12, 77)),     # deepening dusk
    (TOP_TEXEL, (10, 6, 53)),  # indigo: never black
]


def colour_at(x):
    for (x0, c0), (x1, c1) in zip(STOPS, STOPS[1:]):
        if x <= x1:
            t = (x - x0) / (x1 - x0)
            return tuple(round(c0[i] + (c1[i] - c0[i]) * t) for i in range(3))
    return STOPS[-1][1]  # past the last stop: hold it


row_colours = [colour_at(x) for x in range(WIDTH)]
palette = sorted(set(row_colours), key=row_colours.index)  # in gradient order
row = bytes(palette.index(c) for c in row_colours)
pixels = row * HEIGHT

header = struct.pack('<BBBHHBHHHHBB',
                     0,              # no image ID
                     1,              # has a colour map (palette)
                     1,              # uncompressed, colour-mapped
                     0, len(palette), 24,  # palette: first entry, count, bits per entry
                     0, 0, WIDTH, HEIGHT,
                     8,              # bits per pixel
                     0x20)           # top-left origin, like the original
tga_palette = b''.join(bytes((b, g, r)) for (r, g, b) in palette)  # TGA stores BGR

with open('cd/SKY.TGA', 'wb') as f:
    f.write(header + tga_palette + pixels)
print(f'cd/SKY.TGA: {WIDTH}x{HEIGHT}, {len(palette)} colours, indigo from texel {TOP_TEXEL}')
