"""
Works out, for every roof in Work RAM's city, which towers can be seen from it, and writes the
table the game draws by (src/game/city_seen.c; level.c, grey_boxes_add).

Why: VDP1 has no depth buffer, so a tower hidden behind others is still filled, and down among the
towers nearly all of them are hidden. What hides what depends only on where you stand, never on
which way you look, and the city doesn't change: so it's worked out here, once, not every frame.
The game leaves out a tower whose bit is clear with one bit test.

The table must never leave out a tower that can be seen (a hole in the picture). It may keep one
that can't (VDP1 fills it for nothing). Until 2026-11-16 the rule was one hider at a time (the
game's game/skyline.h rule): a tower was hidden only if one tower, pair or block covered all of it.
In Low Work RAM a lane's wall is broken by a street every two towers, so no one piece covers what's
behind it: a roof was left seeing 161 of the 256 towers on average. Together the walls hide most of
the city. So now (the user, 2026-11-16) the walls work together, and the table is still sure:

1. From one point, an eye. Each hider H (a tower, two side by side in a block, or a whole block,
   as low as its lowest) covers, from the eye, an interval of angles. Along any ray in that
   interval, the ray crosses H somewhere between H's nearest and farthest distance (dmin, dmax), and
   anything on that ray seen at an upward slope below t_H is behind H: t_H = (top - eye) / dmax if
   H's top is above the eye, (top - eye) / dmin if not (the slope's weakest, over the whole box).
   A tower B is hidden if every angle B covers is covered by some H with:
     - H wholly nearer than B (H's dmax <= B's dmin): it crosses the ray before B can;
     - t_H above B's steepest slope ((top - eye) / dmin above the eye, / dmax below it): every
       point of B, on any ray, is below what H blocks.
   Different hiders can cover different parts of B: that's the walls together. A gap between
   them (a street) leaves angles uncovered, and B is seen through it, as it really is.
2. From a patch of roof (250 x 250): Wonka, Wimmer and Schmalstieg's occluder shrinking ("Visibility
   preprocessing with occluder fusion for urban walkthroughs", 2000). Every hider shrunk by EPS on
   each side and lowered by EPS: what's hidden from a point by the shrunk hiders is hidden from
   every eye within EPS of that point by the real ones (a line from a nearby eye passes within EPS
   of the first line, inside the real hider). Points every 41.7 cover the patch to within 29.5:
   EPS 30, 49 points a patch, and a tower is seen from the patch if it's seen from any of them.
3. Height: the eye as high as it gets on that roof (CITY_SEEN_EYE over it: standing, and a jump).
   The city's towers all stand from the ground: lower, an eye's line to a point runs lower all along
   it, and sees no more.
4. Checked: lines of sight from random eyes on each roof (every height up to the top) to points on
   every tower, against every box as it is (not shrunk). A tower a line reaches must have its bit
   set, or the tool stops (check_sure). tests/test_city_seen.c does the same in C.

Only roofs you can stand on get a worked out row: a guard page's row is all seen (never used).
And from the corridor's doorway (city_seen_doorway): the same, from points along it. And over the
streets (city_seen_street, 2026-11-18): the same, from pieces of street, for a jump across one.

The towers are read from src/game/city.c's own table, so there's one source. Run it again whenever
that table changes (a few minutes). Usage (from the project folder):  python tools/make_city_seen.py
"""
import math
import re

import numpy as np

CORRIDOR_LENGTH = 4500   # game/corridor.h
SIZE = 500               # CITY_TOWER_SIZE
STREET = 200             # CITY_STREET
COLUMNS, ROWS = 16, 16   # CITY_COLUMNS, CITY_ROWS
ENTRY_ROW = 8            # CITY_ENTRY_ROW
EYE = 200                # CITY_SEEN_EYE (game/city.h)
PATCHES = 2              # CITY_SEEN_PATCHES: a roof's cut into this many each way
GROUND = -3000           # CITY_GROUND
SPACING = 250 / 6        # sample points a patch: every 41.7, so 7 x 7
EPS = 30                 # the hiders' shrink: over SPACING / sqrt(2) = 29.5
# (Measured on 8 roofs, 2026-11-16: points every 125 with EPS 89, 115 towers seen from a patch;
# every 62.5 with 45, 81; every 41.7 with 30, 77; every 25 with 18, 72. The shrink widens every
# street's slit, and the slits are what's seen through. Lines of sight from 25 points over a patch
# reach ~45: what's really seen)
DOORWAY_WIDE = 500       # CITY_DOORWAY_WIDE
DOORWAY_EYE = 160 + EYE  # CITY_DOORWAY_EYE


def col_at(c):
    return CORRIDOR_LENGTH + c * SIZE + (c >> 1) * STREET


def row_at(r):
    return r * SIZE + (r >> 1) * STREET - (ENTRY_ROW * SIZE + (ENTRY_ROW >> 1) * STREET)


source = open('src/game/city.c').read()
found = re.findall(r'TOWER\((\d+), (\d+), (-?\d+), (CITY_\w+)\)', source)
if len(found) != COLUMNS * ROWS:
    raise SystemExit('city.c has %d towers, not %d' % (len(found), COLUMNS * ROWS))
if [(int(c), int(r)) for c, r, _, _ in found] != [(c, r) for r in range(ROWS) for c in range(COLUMNS)]:
    raise SystemExit("city.c's towers aren't row by row")
# (x_min, x_max, y_min, y_max, top): x out along the corridor, y across it
towers = [(col_at(int(c)), col_at(int(c)) + SIZE, row_at(int(r)), row_at(int(r)) + SIZE, int(h)) for c, r, h, _ in found]
standable = [k != 'CITY_GUARD' for _, _, _, k in found]
N = len(towers)
T = np.array(towers, float)


def at(c, r):
    return towers[r * COLUMNS + c]


def union(cells):
    return (min(t[0] for t in cells), max(t[1] for t in cells), min(t[2] for t in cells),
            max(t[3] for t in cells), min(t[4] for t in cells))


# The hiders: every tower; each block's two rows side by side, two columns one above the other,
# and the whole block, each as low as its lowest (solid boxes: towers touch within a block). Then
# shrunk by EPS each side and lowered by EPS (2: the patch)
hiders = list(towers)
for br in range(0, ROWS, 2):
    for bc in range(0, COLUMNS, 2):
        for r in (br, br + 1):
            hiders.append(union([at(bc, r), at(bc + 1, r)]))
        for c in (bc, bc + 1):
            hiders.append(union([at(c, br), at(c, br + 1)]))
        hiders.append(union([at(c, r) for r in (br, br + 1) for c in (bc, bc + 1)]))
H = np.array([(x0 + EPS, x1 - EPS, y0 + EPS, y1 - EPS, top - EPS) for x0, x1, y0, y1, top in hiders], float)


def rect_view(R, ex, ey):
    """For boxes R (x0, x1, y0, y1, ...) seen from (ex, ey): the angle to each one's middle, its
    angular half-interval around it (lo, hi, relative), its nearest and farthest distance, and
    whether the eye is over it (no angles then)"""
    x0, x1, y0, y1 = R[:, 0] - ex, R[:, 1] - ex, R[:, 2] - ey, R[:, 3] - ey
    over = (x0 <= 0) & (x1 >= 0) & (y0 <= 0) & (y1 >= 0)
    mid = np.arctan2((y0 + y1) / 2, (x0 + x1) / 2)
    rel = []
    for cx, cy in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        a = np.arctan2(cy, cx) - mid
        rel.append((a + np.pi) % (2 * np.pi) - np.pi)
    rel = np.array(rel)
    lo, hi = rel.min(axis=0), rel.max(axis=0)
    nx = np.clip(0, x0, x1)
    ny = np.clip(0, y0, y1)
    dmin = np.hypot(nx, ny)
    dmax = np.max([np.hypot(cx, cy) for cx, cy in ((x0, y0), (x1, y0), (x1, y1), (x0, y1))], axis=0)
    return mid, lo, hi, dmin, dmax, over


def seen_from_point(ex, ey, ez):
    """Which towers can be seen from the eye at (ex, ey, ez), by the shrunk hiders (1, above).
    Errs towards seen: a tower's hidden only when it's sure"""
    hm, hlo, hhi, hdmin, hdmax, hover = rect_view(H, ex, ey)
    top = H[:, 4] - ez
    t = np.where(top > 0, top / np.maximum(hdmax, 1e-9), top / np.maximum(hdmin, 1e-9))
    usable = ~hover & (hdmin > 0)
    bm, blo, bhi, bdmin, bdmax, bover = rect_view(T, ex, ey)
    btop = T[:, 4] - ez
    slope = np.where(btop > 0, btop / np.maximum(bdmin, 1e-9), btop / np.maximum(bdmax, 1e-9))
    # [hider, tower]: hider wholly nearer, and blocking above the tower's steepest slope
    q = usable[:, None] & (hdmax[:, None] <= bdmin[None, :]) & (t[:, None] > slope[None, :])
    seen = np.ones(N, bool)
    for j in range(N):
        if bover[j] or bdmin[j] <= 0:
            continue  # stood over it (your own roof): seen
        idx = np.nonzero(q[:, j])[0]
        if len(idx) == 0:
            continue
        # The hiders' intervals, about the tower's middle direction
        d = (hm[idx] - bm[j] + np.pi) % (2 * np.pi) - np.pi
        s, e = d + hlo[idx], d + hhi[idx]
        order = np.argsort(s)
        reach, goal = blo[j], bhi[j]
        for k in order:
            if s[k] > reach:
                break  # a gap: seen through it
            if e[k] > reach:
                reach = e[k]
                if reach >= goal:
                    break
        if reach >= goal:
            seen[j] = False
    return seen


def seen_from_patch(x0, y0, size, ez, size_y=None):
    """From a patch (square, or size x size_y): the towers seen from any of its sample points (2,
    above). Points at most SPACING apart each way, so every point of the patch is within EPS of one"""
    size_y = size if size_y is None else size_y
    nx, ny = int(math.ceil(size / SPACING - 1e-9)), int(math.ceil(size_y / SPACING - 1e-9))
    seen = np.zeros(N, bool)
    for i in range(nx + 1):
        for k in range(ny + 1):
            seen |= seen_from_point(x0 + i * size / nx, y0 + k * size_y / ny, ez)
    return seen


BYTES = (N + 7) >> 3


def seen_bit(t):
    """Tower t's bit in a row (city.c, city_seen_bit): block by block, as level.c lists the boxes
    (2 x 2, the blocks row by row), each block's four towers in a nibble: (c, r), (c + 1, r),
    (c, r + 1), (c + 1, r + 1). So a whole block's tested at once, and no table maps a box to its
    tower (the user, 2026-11-18: the boxes' reads)"""
    c, r = t % COLUMNS, t // COLUMNS
    return ((r >> 1) * (COLUMNS >> 1) + (c >> 1)) * 4 + (r & 1) * 2 + (c & 1)


def bits_of(seen):
    b = [0] * BYTES
    for t in np.nonzero(seen)[0]:
        bit = seen_bit(int(t))
        b[bit >> 3] |= 1 << (bit & 7)
    return b


step = SIZE // PATCHES
table = np.ones((N, PATCHES * PATCHES, N), bool)
for c in range(N):
    if not standable[c]:
        continue  # a guard page: nobody stands there; its row stays all seen
    roof = towers[c]
    for patch in range(PATCHES * PATCHES):
        x0 = roof[0] + (patch % PATCHES) * step
        y0 = roof[2] + (patch // PATCHES) * step
        seen = seen_from_patch(x0, y0, step, roof[4] + EYE)
        seen[c] = True
        table[c, patch] = seen

# --- The streets (the user, 2026-11-18: "Do the streets seen tables") ----------------------------
# Jumping a street, you're over no roof: before this the game had no table there and drew every
# tower down to the ground, 606k pixels a frame mid-jump against VDP1's ~350k, and the nearest
# roofs went missing (city_traverse.steps). So the streets get a table too, worked out the same way.
# A street's cut into pieces: PIECE (250) along it, a roof patch's length, and the whole street's
# width (200) across; where two streets cross, the crossing's one piece (200 x 200). In city.h's
# order: the streets between block columns (OUT_STREETS: each at one out, running across), then
# those between block rows (running out), then the crossings
BLOCKS = COLUMNS // 2            # 8 blocks each way
STREETS = BLOCKS - 1             # CITY_STREETS: 7 streets between them, each way
PIECE = SIZE // PATCHES          # 250 along a street
ALONG = ROWS * PATCHES           # CITY_STREET_ALONG: 32 pieces down a street
STREET_PIECES = 2 * STREETS * ALONG + STREETS * STREETS  # CITY_STREET_PIECES: 497
NO_EYE = -32768                  # CITY_STREET_NO_EYE: no roof beside it you could jump from


def street_piece(p):
    """Piece p's rectangle (x0, y0, width, height), and the towers beside it you could jump over
    it from (c, r)"""
    if p < STREETS * ALONG:  # between block columns s and s + 1: out's fixed, along is across
        s, k = divmod(p, ALONG)
        r = k // PATCHES
        x0 = col_at(2 * s + 2) - STREET
        return ((x0, row_at(r) + (k % PATCHES) * PIECE, STREET, PIECE),
                [(c, rr) for c in (2 * s + 1, 2 * s + 2) for rr in (r - 1, r, r + 1) if 0 <= rr < ROWS])
    p -= STREETS * ALONG
    if p < STREETS * ALONG:  # between block rows: across is fixed, along is out
        s, k = divmod(p, ALONG)
        c = k // PATCHES
        y0 = row_at(2 * s + 2) - STREET
        return ((col_at(c) + (k % PATCHES) * PIECE, y0, PIECE, STREET),
                [(cc, r) for r in (2 * s + 1, 2 * s + 2) for cc in (c - 1, c, c + 1) if 0 <= cc < COLUMNS])
    p -= STREETS * ALONG
    sy, sx = divmod(p, STREETS)  # a crossing: the four corners round it
    return ((col_at(2 * sx + 2) - STREET, row_at(2 * sy + 2) - STREET, STREET, STREET),
            [(c, r) for c in (2 * sx + 1, 2 * sx + 2) for r in (2 * sy + 1, 2 * sy + 2)])


def street_eye(p):
    """How high an eye gets over piece p: as high as over the highest roof beside it you can stand
    on (a jump from there: CITY_SEEN_EYE over it). A lower eye sees no more (3, above). NO_EYE if
    every tower beside it is a guard page: nobody's ever over it"""
    tops = [at(c, r)[4] for c, r in street_piece(p)[1] if standable[r * COLUMNS + c]]
    return max(tops) + EYE if tops else NO_EYE


street_eyes = [street_eye(p) for p in range(STREET_PIECES)]
street_table = np.ones((STREET_PIECES, N), bool)
for p in range(STREET_PIECES):
    if street_eyes[p] == NO_EYE:
        continue  # never over it: its row stays all seen (never used)
    (x0, y0, w, h), _ = street_piece(p)
    street_table[p] = seen_from_patch(x0, y0, w, street_eyes[p], h)

# From the corridor's doorway: points every SPACING along it (each within EPS of every point
# between), the eye as high as it gets there
doorway_seen = np.zeros(N, bool)
for i in range(int(round(DOORWAY_WIDE / SPACING)) + 1):
    doorway_seen |= seen_from_point(CORRIDOR_LENGTH, min(i * SPACING, DOORWAY_WIDE), DOORWAY_EYE)

# --- 4: checked against lines of sight, with the boxes as they are ------------------------------
BOXES = np.array([[x0, x1, y0, y1, GROUND, top] for x0, x1, y0, y1, top in towers], float)
samples, owner = [], []
for i, (x0, x1, y0, y1, top) in enumerate(towers):
    # 5 x 5 over its footprint (its top, its sides' edges and between), at its top and lower down
    for x in np.linspace(x0 + 1, x1 - 1, 5):
        for y in np.linspace(y0 + 1, y1 - 1, 5):
            for z in (top - 1, top - 200, top - 800):
                samples.append((x, y, z))
                owner.append(i)
SAMPLES, OWNER = np.array(samples, float), np.array(owner)


def reached_by_lines(ex, ey, ez):
    """The towers a straight line from the eye reaches, without passing through any other box"""
    d = SAMPLES - np.array([ex, ey, ez])
    lo = np.full((len(SAMPLES), N), 1e-6)
    hi = np.full((len(SAMPLES), N), 1 - 1e-6)
    with np.errstate(divide='ignore', invalid='ignore'):
        for axis, (a, b) in enumerate(((0, 1), (2, 3), (4, 5))):
            e = (ex, ey, ez)[axis]
            inv = 1.0 / d[:, axis][:, None]
            t1, t2 = (BOXES[:, a][None, :] - e) * inv, (BOXES[:, b][None, :] - e) * inv
            near, far = np.minimum(t1, t2), np.maximum(t1, t2)
            flat = d[:, axis][:, None] == 0
            inside = (BOXES[:, a][None, :] < e) & (e < BOXES[:, b][None, :])
            near = np.where(flat, np.where(inside, -np.inf, np.inf), near)
            far = np.where(flat, np.where(inside, np.inf, -np.inf), far)
            lo, hi = np.maximum(lo, near), np.minimum(hi, far)
    blocked = lo < hi
    blocked[np.arange(len(SAMPLES)), OWNER] = False
    return set(OWNER[~blocked.any(axis=1)].tolist())


def check_sure(rounds):
    rnd = np.random.default_rng(16112026)
    roofs = [c for c in range(N) if standable[c]]
    for _ in range(rounds):
        c = roofs[rnd.integers(len(roofs))]
        roof = towers[c]
        ex, ey = rnd.uniform(roof[0], roof[1]), rnd.uniform(roof[2], roof[3])
        ez = roof[4] + rnd.uniform(40, EYE)
        patch = (1 if ex - roof[0] >= step else 0) + (PATCHES if ey - roof[2] >= step else 0)
        for b in reached_by_lines(ex, ey, ez):
            if not table[c, patch, b]:
                raise SystemExit('NOT SURE: tower %d reached by a line from roof %d (%.0f, %.0f, %.0f), '
                                 'but its bit is clear' % (b, c, ex, ey, ez))
    # Over the streets: random eyes on random pieces, from its eye down to 400 under it (mid-jump)
    pieces = [p for p in range(STREET_PIECES) if street_eyes[p] != NO_EYE]
    for _ in range(rounds):
        p = pieces[rnd.integers(len(pieces))]
        (x0, y0, w, h), _ = street_piece(p)
        ex, ey = rnd.uniform(x0, x0 + w), rnd.uniform(y0, y0 + h)
        ez = street_eyes[p] - rnd.uniform(0, 400)
        for b in reached_by_lines(ex, ey, ez):
            if not street_table[p, b]:
                raise SystemExit('NOT SURE: tower %d reached by a line from street piece %d (%.0f, %.0f, %.0f), '
                                 'but its bit is clear' % (b, p, ex, ey, ez))
    for _ in range(rounds // 10):
        ey, ez = rnd.uniform(0, DOORWAY_WIDE), rnd.uniform(0, DOORWAY_EYE)
        for b in reached_by_lines(CORRIDOR_LENGTH, ey, ez):
            if not doorway_seen[b]:
                raise SystemExit('NOT SURE: tower %d reached from the doorway, but its bit is clear' % b)


check_sure(400)

lines, counts = [], []
for c in range(N):
    rows = []
    for patch in range(PATCHES * PATCHES):
        if standable[c]:
            counts.append(int(table[c, patch].sum()))
        rows.append('{%s}' % ', '.join('0x%02x' % v for v in bits_of(table[c, patch])))
    lines.append('\t{ // (%d, %d)%s\n\t\t%s},' % (c % COLUMNS, c // COLUMNS, '' if standable[c] else ' a guard page: never stood on',
                                                  ',\n\t\t'.join(rows)))

with open('src/game/city_seen.c', 'w', newline='\n') as f:
    f.write('''#include "city.h"

// Generated: python tools/make_city_seen.py (it has the how and why). Don't edit: change
// city.c's towers and run it again. For each roof (row by row, as city_towers) and each of its
// patches (city.h: city_seen_patch), a bit for every tower that can be seen from somewhere on
// that patch, block by block (city.h: city_seen_bit, city_seen_has)
const unsigned char city_seen[CITY_TOWERS][CITY_SEEN_PATCHES * CITY_SEEN_PATCHES][CITY_SEEN_BYTES] = {
''' + '\n'.join(lines) + '''
};

// And through the corridor's doorway, from inside it (city.h): the same bit for each tower
const unsigned char city_seen_doorway[CITY_SEEN_BYTES] = {%s};
''' % ', '.join('0x%02x' % v for v in bits_of(doorway_seen)))
    street_lines, street_counts = [], []
    for p in range(STREET_PIECES):
        (x0, y0, w, h), _ = street_piece(p)
        if street_eyes[p] != NO_EYE:
            street_counts.append(int(street_table[p].sum()))
        street_lines.append('\t{%s}, // %d: out %d, across %d%s' % (
            ', '.join('0x%02x' % v for v in bits_of(street_table[p])), p, x0, y0,
            '' if street_eyes[p] != NO_EYE else ': only guard pages beside it'))
    f.write('''
// And over the streets (city.h: city_street_under has the pieces' order): the same bit for each
// tower, seen from somewhere over that piece with the eye up to its city_street_eye
const unsigned char city_seen_street[CITY_STREET_PIECES][CITY_SEEN_BYTES] = {
''' + '\n'.join(street_lines) + '''
};

// How high an eye gets over each piece (as a roof's height): CITY_SEEN_EYE over the highest roof
// beside it you could jump from. CITY_STREET_NO_EYE: only guard pages beside it
const short city_street_eye[CITY_STREET_PIECES] = {
''' + ',\n'.join('\t' + ', '.join(str(e) for e in street_eyes[i:i + 16]) for i in range(0, STREET_PIECES, 16)) + '''
};
''')
street_counts.sort()
print('  over a street piece: median %d seen, 90%% %d, worst %d; %d bytes'
      % (street_counts[len(street_counts) // 2], street_counts[int(len(street_counts) * 0.9)], street_counts[-1],
         STREET_PIECES * (BYTES + 2)))
counts.sort()
print('src/game/city_seen.c: from a roof patch, of %d towers: median %d seen, 90%% %d, worst %d; %d bytes'
      % (N, counts[len(counts) // 2], counts[int(len(counts) * 0.9)], counts[-1], N * PATCHES * PATCHES * BYTES))
print('  through the doorway: %d; checked against %d lines of sight from random eyes' % (int(doorway_seen.sum()), 400 * len(SAMPLES)))
