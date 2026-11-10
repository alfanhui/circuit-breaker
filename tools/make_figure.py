"""
Generates you, as the mirror shows you (src/level.c, level_mirror_figure): on the cycle you were
only energy (the cube); on foot, after the crash, you're a person. A slick, faceless humanoid,
seen from the front: a body of dark glossy glass, its outline caught in a cyan rim of light, and
thin lines of light down the suit (the art direction: bold shapes, a few vibrant colours, extreme
contrast). The head's a smooth oval with no face, only a sheen across where the eyes would be.

Drawn flat (a sprite stood in the reflection, always facing you): the mirror shows you head on.
  cd/FIGURE.TGA   32 x 96, 6 KB in VDP1. Magenta (255, 0, 255) round it: jo makes it see-through
Loaded in the hidden load at the breach (input.c), with the glass.
Usage (from the project folder):  python tools/make_figure.py
"""
import struct

WIDTH, HEIGHT = 32, 96   # ~0.8 world units a texel: the figure's 76 tall (WALKER_HEAD), ~24 across
KEY = (255, 0, 255)      # see-through
BODY = (14, 16, 30)      # dark glossy glass
SHEEN = (40, 46, 78)     # the gloss: lighter down one side
RIM = (110, 235, 255)    # the outline, lit
LINE = (60, 170, 230)    # the suit's lines of light
VISOR = (70, 90, 140)    # the faceless head's sheen

CX = WIDTH / 2


def capsule(x, y, x0, y0, x1, y1, r):
    """Whether (x, y) is within r of the segment (x0, y0) .. (x1, y1)"""
    dx, dy = x1 - x0, y1 - y0
    t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy)))
    px, py = x0 + t * dx, y0 + t * dy
    return (x - px) ** 2 + (y - py) ** 2 <= r * r


def inside(x, y):
    """The figure's shape, in texels (row 0 is the top of the head)"""
    # The head: an oval, a little narrower than tall
    if ((x - CX) / 5.2) ** 2 + ((y - 9.0) / 7.5) ** 2 <= 1:
        return True
    # The neck
    if abs(x - CX) <= 2.2 and 15 <= y <= 19:
        return True
    # The torso: shoulders 22 across tapering to a waist 13, then hips 15
    if 18 <= y <= 52:
        if y < 22:
            half = 6 + (y - 18) * 1.25            # the shoulders' slope
        elif y < 44:
            half = 11 - (y - 22) * (4.5 / 22)     # chest to waist
        else:
            half = 6.5 + (y - 44) * 0.15          # the hips
        if abs(x - CX) <= half:
            return True
    # The arms, hanging a little out from the sides
    for side in (-1, 1):
        if capsule(x, y, CX + side * 10.5, 21, CX + side * 12.0, 38, 2.6):   # upper
            return True
        if capsule(x, y, CX + side * 12.0, 38, CX + side * 12.8, 54, 2.2):   # fore
            return True
        if capsule(x, y, CX + side * 12.8, 54, CX + side * 12.8, 58, 2.4):   # the hand
            return True
    # The legs, a little apart
    for side in (-1, 1):
        if capsule(x, y, CX + side * 3.6, 52, CX + side * 4.2, 74, 3.4):     # thigh
            return True
        if capsule(x, y, CX + side * 4.2, 74, CX + side * 4.4, 92, 2.6):     # shin
            return True
        if capsule(x, y, CX + side * 4.4, 92, CX + side * 5.6, 94, 2.2):     # the foot
            return True
    return False


def texel(x, y):
    cx, cy = x + 0.5, y + 0.5
    if not inside(cx, cy):
        return KEY
    # The rim: a texel inside with a neighbour outside catches the light
    for nx, ny in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if not inside(cx + nx, cy + ny):
            return RIM
    # The faceless head's sheen: a band across where the eyes would be
    if 6.5 <= cy <= 9.5 and abs(cx - CX) <= 4:
        return VISOR
    # The suit's lines of light: down the middle of the chest, and from each shoulder to the hip
    if 20 <= cy <= 50 and abs(cx - CX) <= 0.5:
        return LINE
    for side in (-1, 1):
        if 22 <= cy <= 50 and abs(cx - (CX + side * (8.0 - (cy - 22) * 0.12))) <= 0.5:
            return LINE
        if 54 <= cy <= 88 and abs(cx - (CX + side * (4.0 + (cy - 54) * 0.015))) <= 0.5:
            return LINE   # down each leg
    # The gloss: lighter on the left of the body, as if lit from there
    if cx < CX - 2 and (cx + cy * 0.08) % 7 < 2:
        return SHEEN
    return BODY


rows = [[texel(x, y) for x in range(WIDTH)] for y in range(HEIGHT)]
# 24-bit, bottom row first, BGR (as tools/make_glass.py)
pixels = bytearray(c for row in reversed(rows) for colour in row for c in reversed(colour))
header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, WIDTH, HEIGHT, 24, 0)
with open('cd/FIGURE.TGA', 'wb') as f:
    f.write(header + pixels)
print(f'cd/FIGURE.TGA: {WIDTH}x{HEIGHT}, {WIDTH * HEIGHT * 2 // 1024} KB in VDP1')
# A look at it in the terminal: # the body, + the light, . see-through
for row in rows[::2]:
    print(''.join('.' if c == KEY else '+' if c in (RIM, LINE, VISOR) else '#' for c in row))
