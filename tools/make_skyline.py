"""
Generates the city that stands all round, far off: the skyline (src/level.c, skyline_add: a ring
of flat strips round you, textured with this, following you part of the way as you move so it
slides against everything nearer: parallax).

One texture, the depth inside it (third pass, 2026-11-12: three rings, each its own layer, cost
three times the polygons and the user saw little difference between them): three rows of towers
drawn one over another, the far ones first. The look is Tron's: near-black monoliths, tall and
thin, sharp tops (flat, slanted, set back, a needle), no windows; light only along edges, a thin
rim or a single trace of dim cyan on a few near towers, fainter on the middle row. The rows part
by depth as air does: the near row nearly black, the far one pale, a violet haze near the sky's.

Its bottom quarter is solid black, the near row's feet: the strip reaches down to the floor, so
there's never sky under it (the arena's walls sink between rounds: under a skyline that started
higher up, the horizon showed through).

  cd/SKYLINE.TGA   128 x 64, 16 KB of VDP1 RAM
Usage (from the project folder):  python tools/make_skyline.py
The same towers every run (fixed seeds).
"""
import random
import struct

WIDTH, HEIGHT = 128, 64
BASE = 16  # the solid black rows at the bottom
KEY = (255, 0, 255)  # see-through
BLACK = (4, 4, 10)

# The rows, far to near: the towers' two shades (every other tower), the light on edges (None:
# none) and how often a tower has it, the narrowest and widest tower, the lowest and highest top
# (rows above the black base), and how often there's a gap between towers
ROWS = (
    {'seed': 29, 'body': (46, 36, 76), 'alt': (50, 39, 82), 'light': None, 'lit': 0.0,
     'width': (3, 7), 'top': (18, 46), 'gap': 0.05},
    {'seed': 17, 'body': (20, 16, 38), 'alt': (24, 19, 44), 'light': (52, 58, 104), 'lit': 0.3,
     'width': (3, 9), 'top': (10, 40), 'gap': 0.15},
    {'seed': 5, 'body': (6, 6, 14), 'alt': (10, 9, 20), 'light': (34, 104, 128), 'lit': 0.45,
     'width': (4, 12), 'top': (3, 34), 'gap': 0.3},
)

rows = [[KEY] * WIDTH for _ in range(HEIGHT)]  # row 0 is the top
GROUND = HEIGHT - BASE  # the row the towers stand on


def put(x, y, c):
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        rows[y][x] = c


for spec in ROWS:
    rnd = random.Random(spec['seed'])
    x = 0
    shade = 0
    while x < WIDTH:
        if rnd.random() < spec['gap']:
            x += rnd.randint(1, 3)  # a street seen end on: what's behind shows
            continue
        w = min(rnd.randint(*spec['width']), WIDTH - x)
        top = rnd.randint(*spec['top'])
        colour = spec['body'] if shade == 0 else spec['alt']
        shade ^= 1
        kind = rnd.choice(('flat', 'flat', 'slant', 'setback', 'needle'))
        heights = []
        for i in range(w):
            h = top
            if kind == 'slant' and w > 1:
                h = top - (i * 6) // w  # falls away to one side
            elif kind == 'setback' and w >= 4 and (i == 0 or i == w - 1):
                h = top - 5  # a step in at the top
            heights.append(max(2, h))
        for i, h in enumerate(heights):
            for ty in range(GROUND - h, GROUND):
                put(x + i, ty, colour)
        if kind == 'needle':
            mid = x + w // 2
            for ty in range(GROUND - top - rnd.randint(5, 10), GROUND - top):
                put(mid, ty, colour)
        if spec['light'] and rnd.random() < spec['lit']:
            for i, h in enumerate(heights):
                put(x + i, GROUND - h, spec['light'])  # a rim along its top
        if spec['light'] and w >= 3 and rnd.random() < spec['lit'] * 0.6:
            tx = x + rnd.randint(1, w - 2)
            for ty in range(GROUND - heights[tx - x] + 3, GROUND):
                put(tx, ty, spec['light'])  # a trace up its face
        x += w

for ty in range(GROUND, HEIGHT):
    for tx in range(WIDTH):
        rows[ty][tx] = BLACK

# 24-bit, bottom row first, BGR (as tools/make_glass.py)
pixels = bytearray(c for row in reversed(rows) for colour in row for c in reversed(colour))
header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, WIDTH, HEIGHT, 24, 0)
with open('cd/SKYLINE.TGA', 'wb') as f:
    f.write(header + pixels)
print(f'cd/SKYLINE.TGA: {WIDTH}x{HEIGHT}, {WIDTH * HEIGHT * 2 // 1024} KB in VDP1')
for row in rows[::2]:
    print(''.join('.' if c == KEY else '#' for c in row))
