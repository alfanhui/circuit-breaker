"""
Works out, for every roof in Work RAM's city, which towers can be seen from it, and writes the
table the game draws by (src/game/city_seen.c; level.c, grey_boxes_add).

Why: VDP1 has no depth buffer, so a tower hidden behind another is still filled, and down among
the towers nearly all of them are hidden. The game was working that out every frame (pick 8
hiders, test all 84 towers against them: game/skyline.h) and it cost a third of a frame at the
CD block's hub (2026-11-13). But what hides what depends only on where you stand, never on which
way you look, and the city doesn't change: so it's worked out here, once, with every tower as a
hider instead of eight.

The rule is the game's own (game/skyline.h), on the floor plan:
  hider A hides tower B from an eye when every corner of B is inside A's wedge from the eye and
  beyond the line through A's two silhouette corners, A's top is no lower than B's, and A's top is
  no lower than the eye.
From a patch of roof: the places A hides B from are a convex patch of the plan (they're bounded
by straight lines through the two boxes' corners), so if the same A hides B from all four corners
of a rectangle, it hides it from everywhere in it. A whole roof (500 square) is too wide for that
to hide much: from the hub's, 65 of the 84 towers were still seen. So each roof is four patches
(PATCHES x PATCHES, 250 square): 40 from the hub's, for 3.7 KB. (From its very middle, 29: the
game picks hiders for where you stand among those the table leaves, each frame, for the rest.)
The eye's taken as high as it gets on that roof (CITY_SEEN_EYE over it: standing, and a jump).
Hiders: every tower, and every block of 2 x 2 (as low as its lowest tower).

The towers are read from src/game/city.c's own table, so there's one source. Run it again
whenever that table changes: the unit test (tests/test_city_seen.c) works the table out again
with the game's code and fails if this file's is stale.
Usage (from the project folder):  python tools/make_city_seen.py
"""
import re

CORRIDOR_LENGTH = 4500   # game/corridor.h
SIZE = 500               # CITY_TOWER_SIZE
STREET = 200             # CITY_STREET
COLUMNS, ROWS = 7, 12
EYE = 200                # CITY_SEEN_EYE (game/city.h)
PATCHES = 2              # CITY_SEEN_PATCHES: a roof's cut into this many each way


def col_at(c):
    return CORRIDOR_LENGTH + c * SIZE + (c >> 1) * STREET


def row_at(r):
    return r * SIZE + (r >> 1) * STREET - (5 * SIZE + 2 * STREET)


source = open('src/game/city.c').read()
found = re.findall(r'TOWER\((\d+), (\d+), (-?\d+), CITY_\w+\)', source)
if len(found) != COLUMNS * ROWS:
    raise SystemExit('city.c has %d towers, not %d' % (len(found), COLUMNS * ROWS))
# (x_min, x_max, y_min, y_max, top): x out along the corridor, y across it
towers = []
for c, r, h in found:
    c, r, h = int(c), int(r), int(h)
    towers.append((col_at(c), col_at(c) + SIZE, row_at(r), row_at(r) + SIZE, h))
if [(int(c), int(r)) for c, r, _ in found] != [(c, r) for r in range(ROWS) for c in range(COLUMNS)]:
    raise SystemExit("city.c's towers aren't row by row")

# The blocks: 2 x 2 towers that touch (the last column's are 1 x 2), as low as their lowest
hiders = list(towers)
for br in range(0, ROWS, 2):
    for bc in range(0, COLUMNS, 2):
        cells = [towers[r * COLUMNS + c] for r in (br, br + 1) for c in (bc, bc + 1) if c < COLUMNS]
        hiders.append((min(t[0] for t in cells), max(t[1] for t in cells), min(t[2] for t in cells),
                       max(t[3] for t in cells), min(t[4] for t in cells)))


def silhouette(ex, ey, box):
    """skyline.c's: the box's two corners furthest apart as the eye sees them, from the eye, b to
    the left of a. None if the eye's over the box (or on its edge)"""
    x_min, x_max, y_min, y_max, _ = box
    side_x = -1 if ex < x_min else 1 if ex > x_max else 0
    side_y = -1 if ey < y_min else 1 if ey > y_max else 0
    if side_x == 0 and side_y == 0:
        return None
    near_x = (x_min if side_x < 0 else x_max) - ex
    far_x = (x_max if side_x < 0 else x_min) - ex
    near_y = (y_min if side_y < 0 else y_max) - ey
    far_y = (y_max if side_y < 0 else y_min) - ey
    if side_y == 0:
        px = qx = near_x
        py, qy = y_min - ey, y_max - ey
    elif side_x == 0:
        py = qy = near_y
        px, qx = x_min - ex, x_max - ex
    else:
        px, py, qx, qy = far_x, near_y, near_x, far_y
    if px * qy - py * qx > 0:
        return px, py, qx, qy
    return qx, qy, px, py


def behind(h, x, y):
    ax, ay, bx, by = h
    if ax * y - ay * x < 0 or x * by - y * bx < 0:
        return False
    return (bx - ax) * (y - ay) - (by - ay) * (x - ax) < 0


def hides_from(ex, ey, a, b):
    h = silhouette(ex, ey, a)
    if h is None:
        return False
    x0, x1, y0, y1 = b[0] - ex, b[1] - ex, b[2] - ey, b[3] - ey
    return behind(h, x0, y0) and behind(h, x1, y0) and behind(h, x1, y1) and behind(h, x0, y1)


def seen(c, patch, b):
    """Whether tower b can be seen from somewhere on roof c's patch (patch: out + PATCHES x across)"""
    if b == c:
        return True
    roof, target = towers[c], towers[b]
    eye_z = roof[4] + EYE
    step = SIZE // PATCHES
    x0 = roof[0] + (patch % PATCHES) * step
    y0 = roof[2] + (patch // PATCHES) * step
    corners = ((x0, y0), (x0 + step, y0), (x0 + step, y0 + step), (x0, y0 + step))
    for a in hiders:
        if a is target or a[4] < target[4] or a[4] < eye_z:
            continue
        if all(hides_from(ex, ey, a, target) for ex, ey in corners):
            return False
    return True


# From inside the corridor, through its doorway (city.h, city_seen_doorway): hidden from both its
# ends (out CORRIDOR_LENGTH, across 0 and DOORWAY_WIDE), the eye up to DOORWAY_EYE
DOORWAY_WIDE = 500       # CITY_DOORWAY_WIDE
DOORWAY_EYE = 160 + EYE  # CITY_DOORWAY_EYE


def seen_through_doorway(b):
    target = towers[b]
    ends = ((CORRIDOR_LENGTH, 0), (CORRIDOR_LENGTH, DOORWAY_WIDE))
    for a in hiders:
        if a is target or a[4] < target[4] or a[4] < DOORWAY_EYE:
            continue
        if all(hides_from(ex, ey, a, target) for ex, ey in ends):
            return False
    return True


COUNT = COLUMNS * ROWS
BYTES = (COUNT + 7) >> 3
lines, total = [], 0
for c in range(COUNT):
    rows = []
    for patch in range(PATCHES * PATCHES):
        bits = [0] * BYTES
        n = 0
        for b in range(COUNT):
            if seen(c, patch, b):
                bits[b >> 3] |= 1 << (b & 7)
                n += 1
        total += n
        rows.append('{%s}' % ', '.join('0x%02x' % v for v in bits))
    lines.append('\t{ // (%d, %d)\n\t\t%s},' % (c % COLUMNS, c // COLUMNS, ',\n\t\t'.join(rows)))

doorway = [0] * BYTES
doorway_count = 0
for b in range(COUNT):
    if seen_through_doorway(b):
        doorway[b >> 3] |= 1 << (b & 7)
        doorway_count += 1

with open('src/game/city_seen.c', 'w', newline='\n') as f:
    f.write('''#include "city.h"

// Generated: python tools/make_city_seen.py (it has the how and why). Don't edit: change
// city.c's towers and run it again. For each roof (row by row, as city_towers) and each of its
// patches (city.h: city_seen_patch), a bit for every tower that can be seen from somewhere on
// that patch: bit (t & 7) of byte (t >> 3)
const unsigned char city_seen[CITY_TOWERS][CITY_SEEN_PATCHES * CITY_SEEN_PATCHES][CITY_SEEN_BYTES] = {
''' + '\n'.join(lines) + '''
};

// And through the corridor's doorway, from inside it (city.h): the same bit for each tower
const unsigned char city_seen_doorway[CITY_SEEN_BYTES] = {%s};
''' % ', '.join('0x%02x' % v for v in doorway))
print('src/game/city_seen.c: %d roofs x %d patches, %d towers seen from one on average (of %d), %d bytes'
      % (COUNT, PATCHES * PATCHES, total // (COUNT * PATCHES * PATCHES), COUNT, COUNT * PATCHES * PATCHES * BYTES))
print('  and through the doorway: %d towers of %d' % (doorway_count, COUNT))
