"""
Work RAM's two banks, planned together: the bigger city (docs/WORK_RAM.md, "A bigger city").

  python tools/ram_banks_plan.py                    ->  docs/images/ram_banks_plan.svg, and a report
  python tools/ram_banks_plan.py --generate BANK N  ->  prints a new maze for BANK's map (seed N)
  python tools/ram_banks_plan.py --quick            ->  the report without the sight count (seconds)

The Saturn has two banks of Work RAM, 1 MB each: Low at 0x0020_0000 (slow DRAM) and High at
0x0600_0000 (fast SDRAM). Each is a district of 16 x 16 towers, a tower a page of 4 KB: the tower at
row r, column c is the page at base + r x 0x1_0000 + c x 0x1000. 512 towers in all, six times the city
built so far (tools/work_ram_plan.py, 84). Not used by the game yet: the plan is agreed first.

The layout (the user, 2026-11-15):
  - The arena's pyramid in the middle, Low Work RAM on one side and High on the other, each with
    its column 0 facing it. The corridor from the arena opens onto the middle of Low's edge (column
    0, row 8): the pyramid falls to the ground exactly where the corridor ends, so a bank can't
    reach back round it.
  - Two data buses, north and south of the pyramid, from each bank's corners nearest it (column 0,
    rows 0 and E) round the pyramid to the other bank's. The rides between the banks.
  - Three of the six ways out in each bank, where the real Saturn has them round Work RAM. Addresses
    run through a bank row by row (64 KB a row, 4 KB a tower along it), so a place whose address
    is below the bank's is by its first page (row 0, the low columns), one above it by its last
    (row E, the high columns), in the order of their addresses. On the memory map:
        0x0018_0000  Backup RAM           Low,  row 0, column 4 (just below Low Work RAM)
        0x0020_0000  LOW WORK RAM
        0x0580_0000  CD block (A-bus)     Low,  row E, column A (above Low)
        0x05A0_0000  SCSP (B-bus)         Low,  row E, column E (above the CD block)
        0x05C0_0000  VDP1 (B-bus)         High, row 0, column 4 (below High Work RAM)
        0x05E0_0000  VDP2 (B-bus)         High, row 0, column 8 (above VDP1)
        0x0600_0000  HIGH WORK RAM
        (out of VDP2) Video output        High, row E: the far end, the last way out
    Split at the middle of the number line, that's the story's order too (STORY.md): the CD block,
    Backup RAM and the SCSP in Low; VDP1, VDP2 and the video output in High, across a bus ride.
  - Lanes a tower wide, walled by guard pages: a maze on the towers at even columns and rows.
  - Heights fall away a block at a time from the bank's way in (Low: the corridor; High: either
    bus): every street level (a jump either way), each block a step of STEP down on its far side
    (a drop one way, a climb back: walker.h climbs up to 100). Every move works both ways, so a
    loop can be run round and round to lose the watchdog.

What it checks, per bank: every roof and hub reached, every move both ways; dead ends, T-junctions,
crossroads, loops, the longest straight lane; and how many towers are seen from each roof (aim:
SIGHT_AIM at most), by the game's table rule today and with the walls working together.

Units are the game's: a tower 500 across (11.5 m), streets 200, the eye 68 up.
"""
import os
import random
import sys

import numpy as np

# --- The maps -----------------------------------------------------------------------------------
# One character a tower: column 0 (by the pyramid) to F, row 0 (north) to F (south).
#   .  a roof            #  a guard page: far too tall to stand on (the lanes' walls)
#   E  the corridor's way in (Low)          n s  the north and south buses' ends
#   C B S 1 2 V  the ways out: CD block, Backup RAM, SCSP, VDP1, VDP2, Video output
# Edit these, not the generator's output
LOW = [  # draft 1's maze (--generate low 12, LOOPS 8), the ways out moved to their addresses
    'n..#B....#...#.#',  # 0
    '.#.#.#.#.#.#.#.#',  # 1
    '.#...#.....#...#',  # 2
    '.#######.#####.#',  # 3
    '...#...#...#...#',  # 4
    '.#.#.#.#.###.#.#',  # 5
    '.#.....#.......#',  # 6
    '##.###.#########',  # 7
    'E#...#.........#',  # 8
    '.#######.#.###.#',  # 9
    '...#...#.....#.#',  # A
    '.#.#.#.###.###.#',  # B
    '.........#.#...#',  # C
    '.###.###.###.#.#',  # D
    's....#....C..#S#',  # E
    '################',  # F
]
HIGH = [  # --generate high 54 (LOOPS 8)
    'n..#1...2#.....#',  # 0
    '##.###.#.#####.#',  # 1
    '...#...#.......#',  # 2
    '.#.#.#.###.#.#.#',  # 3
    '...#.........#.#',  # 4
    '##.###.###.###.#',  # 5
    '.#...#...#.....#',  # 6
    '.###.###.#.#####',  # 7
    '.#...#...#...#.#',  # 8
    '.#.###.#####.#.#',  # 9
    '...#.....#.....#',  # A
    '.#.###.#.#####.#',  # B
    '.#.....#...#...#',  # C
    '.###.#.#.#.#.###',  # D
    's....#.....#..V#',  # E
    '################',  # F
]

HUBS = {'C': 'CD block', 'B': 'Backup RAM', 'S': 'SCSP', '1': 'VDP1', '2': 'VDP2', 'V': 'Video output'}
# Where each bank's ways out go (column, row): see the memory map above
PLACES = {
    'low': {'B': (4, 0), 'C': (10, 14), 'S': (14, 14), 'n': (0, 0), 's': (0, 14), 'E': (0, 8)},
    'high': {'1': (4, 0), '2': (8, 0), 'V': (14, 14), 'n': (0, 0), 's': (0, 14)},
}
BASES = {'low': 0x00200000, 'high': 0x06000000}
WAYS_IN = {'low': 'E', 'high': 'ns'}  # where a bank's heights fall from

SIZE, STREET = 500, 200                  # CITY_TOWER_SIZE, CITY_STREET (game/city.h)
COLUMNS = ROWS = 16
STEP = 90                                # a block's fall on its far side: a clean drop, a climb back
CLEAN_DROP, CLIMB, KERB = 120, 100, 16   # walker.h
GUARD_ABOVE = 400                        # a guard page over the roofs round it
EYE = 200                                # CITY_SEEN_EYE
PATCHES = 2                              # CITY_SEEN_PATCHES
SIGHT_AIM = 40
ARENA_HALF, CORRIDOR_LENGTH = 8000, 4500  # the arena's walls; the corridor out from one
WALL_TOP, GROUND = 1500, -3000            # level.c: the walls' top; game/city.h: CITY_GROUND
PYRAMID_FOOT = ARENA_HALF + (WALL_TOP - GROUND)  # 45 degrees from the walls' top: 12500
BUS_OUT = PYRAMID_FOOT + 1000             # the buses run this far north and south of the middle
ENTRY_ROW = 8
LOOPS = 8
QUICK = '--quick' in sys.argv


def offset(i):
    """A tower's near edge from its bank's own (column or row): 500 a tower, a street every two"""
    return i * SIZE + (i >> 1) * STREET


def tower_rect(bank, col, row):
    """The tower's rectangle in the plan: x east of the arena's middle (Low east, High west, each
    with column 0 at the pyramid's foot), y south, row 8's top edge on the corridor's line"""
    y0 = offset(row) - offset(ENTRY_ROW)
    if bank == 'low':
        x0 = PYRAMID_FOOT + offset(col)
        return x0, x0 + SIZE, y0, y0 + SIZE
    x1 = -PYRAMID_FOOT - offset(col)
    return x1 - SIZE, x1, y0, y0 + SIZE


def steps_between(i, j):
    """Steps down from tower j to tower i along one axis: the boundaries crossed within a block (j
    and j + 1 share one when j is even). A street's two sides share a step: level"""
    lo, hi = min(i, j), max(i, j)
    return sum(1 for k in range(lo, hi) if k % 2 == 0)


# --- Generating a maze (--generate) -------------------------------------------------------------


def generate(bank, seed):
    """A maze on the towers at even columns and rows (the towers between two of them a roof where
    the maze joins them, a guard page where it doesn't; the odd-odd ones always guard pages), from
    the bank's first way in, turning preferred to going straight; then LOOPS more walls opened, and
    the bank's places put in"""
    rnd = random.Random(seed)
    places = PLACES[bank]
    cells = [(c, r) for r in range(0, ROWS - 1, 2) for c in range(0, COLUMNS - 1, 2)]
    grid = [['#'] * COLUMNS for _ in range(ROWS)]
    for c, r in cells:
        grid[r][c] = '.'
    start = places[WAYS_IN[bank][0]]
    seen, stack = {start}, [start]
    while stack:
        c, r = stack[-1]
        ways = [(c + dc, r + dr) for dc, dr in ((2, 0), (-2, 0), (0, 2), (0, -2))
                if (c + dc, r + dr) in cells and (c + dc, r + dr) not in seen]
        if not ways:
            stack.pop()
            continue
        if len(stack) > 1 and len(ways) > 1:
            pc, pr = stack[-2]
            ahead = (2 * c - pc, 2 * r - pr)
            if ahead in ways and rnd.random() < 0.75:
                ways.remove(ahead)
        n = rnd.choice(ways)
        grid[(r + n[1]) // 2][(c + n[0]) // 2] = '.'
        seen.add(n)
        stack.append(n)
    closed = [(c + dc // 2, r + dr // 2) for c, r in cells for dc, dr in ((2, 0), (0, 2))
              if (c + dc, r + dr) in cells and grid[r + dr // 2][c + dc // 2] == '#']
    for c, r in rnd.sample(closed, LOOPS):
        grid[r][c] = '.'
    for ch, (c, r) in places.items():
        grid[r][c] = ch
    return [''.join(row) for row in grid]


if '--generate' in sys.argv:
    i = sys.argv.index('--generate')
    for n, line in enumerate(generate(sys.argv[i + 1], int(sys.argv[i + 2]))):
        print("    '%s',  # %X" % (line, n))
    raise SystemExit
MAPS = {'low': LOW, 'high': HIGH}

# --- A bank: its towers, moves, routes and views ------------------------------------------------


class Tower:
    def __init__(self, bank, col, row, ch):
        self.bank, self.col, self.row, self.ch = bank, col, row, ch
        self.x0, self.x1, self.y0, self.y1 = tower_rect(bank, col, row)
        self.guard = ch == '#'
        self.hub = HUBS.get(ch)
        self.h = 0


def analyse(bank):
    m = MAPS[bank]
    towers = {(c, r): Tower(bank, c, r, ch) for r, line in enumerate(m) for c, ch in enumerate(line)}
    ways_in = [k for k, t in towers.items() if t.ch in WAYS_IN[bank]]
    for t in towers.values():
        # From the nearest way in (the fewest steps down): still level across every street
        t.h = -STEP * min(max(steps_between(t.col, w[0]), steps_between(t.row, w[1])) for w in ways_in)
    for t in towers.values():
        if t.guard:
            near = [n.h for n in towers.values() if not n.guard and abs(n.col - t.col) <= 2 and abs(n.row - t.row) <= 2]
            t.h = max(near + [-STEP * 8]) + GUARD_ABOVE

    def move(a, b):
        if a.guard or b.guard:
            return None
        dz = b.h - a.h
        street = (a.col != b.col and min(a.col, b.col) % 2 == 1) or (a.row != b.row and min(a.row, b.row) % 2 == 1)
        if street:
            return 'jump' if -CLEAN_DROP <= dz <= 0 else None
        if -CLEAN_DROP <= dz <= KERB:
            return 'drop' if dz < -KERB else 'step'
        return 'climb' if dz <= CLIMB else None

    links = {}
    for (c, r), t in towers.items():
        links[(c, r)] = []
        for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            n = towers.get((c + dc, r + dr))
            if n and move(t, n):
                links[(c, r)].append(((n.col, n.row), move(t, n)))
    roofs = [k for k, t in towers.items() if not t.guard]
    start = ways_in[0]
    dist, todo = {start: 0}, [start]
    while todo:
        k = todo.pop(0)
        for n, _ in links[k]:
            if n not in dist:
                dist[n] = dist[k] + 1
                todo.append(n)
    reached = set(dist)
    two_way = all(any(back == a for back, _ in links[b]) for a in roofs for b, _ in links[a])
    places = set(PLACES[bank].values())
    degree = {k: len(links[k]) for k in roofs}
    dead_ends = [k for k in roofs if degree[k] == 1 and k not in places]
    tees = [k for k in roofs if degree[k] == 3]
    crossroads = [k for k in roofs if degree[k] == 4]
    edges = sum(len(v) for k, v in links.items() if k in reached) // 2
    loops = edges - len(reached) + 1

    best = (0, None)
    for line in range(16):
        for along_col in (True, False):
            run = 0
            for i in range(16):
                k = (i, line) if along_col else (line, i)
                run = run + 1 if not towers[k].guard else 0
                if run > best[0]:
                    best = (run, ('row' if along_col else 'column', line, i - run + 1, i))

    sight_rule, sight = {}, {}
    if not QUICK:
        sight_rule, sight = views(bank, towers, roofs, reached)
    return dict(towers=towers, links=links, roofs=roofs, reached=reached, dist=dist, two_way=two_way,
                dead_ends=dead_ends, tees=tees, crossroads=crossroads, loops=loops, straight=best,
                sight_rule=sight_rule, sight=sight)


def views(bank, towers, roofs, reached):
    """How many towers each roof sees (its worst patch): by today's table rule (one hider at a
    time: make_city_seen.py, game/skyline.h), and with the walls together (sight lines, sampled:
    an estimate for the plan, not a table for the game). Within the bank: the other's far off,
    across the pyramid (the skyline and landmarks stand in for it)"""
    order = sorted(towers)
    box = np.array([[towers[k].x0, towers[k].x1, towers[k].y0, towers[k].y1, towers[k].h] for k in order], float)
    hiders = [list(b) for b in box]
    for br in range(0, ROWS, 2):
        for bc in range(0, COLUMNS, 2):
            ts = [towers[(c, r)] for r in (br, br + 1) for c in (bc, bc + 1)]
            hiders.append([min(t.x0 for t in ts), max(t.x1 for t in ts), min(t.y0 for t in ts),
                           max(t.y1 for t in ts), min(t.h for t in ts)])
    extra = []
    if bank == 'low':  # the corridor: half its width (500) either side of its line, its walls' top
        extra = [[ARENA_HALF, PYRAMID_FOOT, -SIZE, SIZE, WALL_TOP]]
    hiders = np.array(hiders + extra, float)
    n = len(order)

    def hidden_from(ex, ey, eye_z):
        x0, x1, y0, y1, top = hiders.T
        sx = np.where(ex < x0, -1, np.where(ex > x1, 1, 0))
        sy = np.where(ey < y0, -1, np.where(ey > y1, 1, 0))
        near_x, far_x = np.where(sx < 0, x0, x1) - ex, np.where(sx < 0, x1, x0) - ex
        near_y, far_y = np.where(sy < 0, y0, y1) - ey, np.where(sy < 0, y1, y0) - ey
        px = np.where(sy == 0, near_x, np.where(sx == 0, x0 - ex, far_x))
        qx = np.where(sy == 0, near_x, np.where(sx == 0, x1 - ex, near_x))
        py = np.where(sy == 0, y0 - ey, near_y)
        qy = np.where(sy == 0, y1 - ey, np.where(sx == 0, near_y, far_y))
        flip = px * qy - py * qx <= 0
        ax, ay = np.where(flip, qx, px)[:, None], np.where(flip, qy, py)[:, None]
        bx, by = np.where(flip, px, qx)[:, None], np.where(flip, py, qy)[:, None]
        ok = ~((sx == 0) & (sy == 0)) & (top >= eye_z)
        hid = np.ones((len(hiders), n), bool)
        for cx, cy in ((0, 2), (1, 2), (1, 3), (0, 3)):
            x, y = box[:, cx][None, :] - ex, box[:, cy][None, :] - ey
            hid &= (ax * y - ay * x >= 0) & (x * by - y * bx >= 0) & ((bx - ax) * (y - ay) - (by - ay) * (x - ax) < 0)
        hid &= ok[:, None] & (hiders[:, 4][:, None] >= box[:, 4][None, :])
        hid[np.arange(n), np.arange(n)] = False
        return hid

    boxes3 = np.array([[t.x0, t.x1, t.y0, t.y1, GROUND, t.h] for t in (towers[k] for k in order)]
                      + [[e[0], e[1], e[2], e[3], GROUND, e[4]] for e in extra], float)
    samples, owner = [], []
    for i, k in enumerate(order):
        t = towers[k]
        x0, x1, y0, y1 = t.x0 + 2, t.x1 - 2, t.y0 + 2, t.y1 - 2
        for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
            for z in (t.h - 2, t.h - 300, t.h - 900):
                samples.append((x, y, z))
                owner.append(i)
        samples.append(((x0 + x1) / 2, (y0 + y1) / 2, t.h - 2))
        owner.append(i)
    samples, owner = np.array(samples, float), np.array(owner)

    def seen_by_rays(ex, ey, ez):
        d = samples - np.array([ex, ey, ez])
        lo = np.full((len(samples), len(boxes3)), 1e-3)
        hi = np.full((len(samples), len(boxes3)), 1 - 1e-3)
        with np.errstate(divide='ignore', invalid='ignore'):
            for axis, (a, b) in enumerate(((0, 1), (2, 3), (4, 5))):
                e = (ex, ey, ez)[axis]
                inv = 1.0 / d[:, axis][:, None]
                t1, t2 = (boxes3[:, a][None, :] - e) * inv, (boxes3[:, b][None, :] - e) * inv
                near, far = np.minimum(t1, t2), np.maximum(t1, t2)
                flat = d[:, axis][:, None] == 0
                inside = (boxes3[:, a][None, :] < e) & (e < boxes3[:, b][None, :])
                near = np.where(flat, np.where(inside, -np.inf, np.inf), near)
                far = np.where(flat, np.where(inside, np.inf, -np.inf), far)
                lo, hi = np.maximum(lo, near), np.minimum(hi, far)
        blocked = lo < hi
        blocked[np.arange(len(samples)), owner] = False
        return len(set(owner[~blocked.any(axis=1)].tolist()))

    step = SIZE // PATCHES
    sight_rule, sight = {}, {}
    for k in roofs:
        if k not in reached:
            continue
        t = towers[k]
        worst_rule = worst = 0
        for p in range(PATCHES * PATCHES):
            x0, y0 = t.x0 + (p % PATCHES) * step, t.y0 + (p // PATCHES) * step
            h = None
            for ex, ey in ((x0, y0), (x0 + step, y0), (x0 + step, y0 + step), (x0, y0 + step)):
                m = hidden_from(ex, ey, t.h + EYE)
                h = m if h is None else (h & m)
            worst_rule = max(worst_rule, n - int(h.any(axis=0).sum()))
            worst = max(worst, seen_by_rays(x0 + step / 2, y0 + step / 2, t.h + EYE))
        sight_rule[k], sight[k] = worst_rule, worst
    return sight_rule, sight


banks = {b: analyse(b) for b in ('low', 'high')}

# --- Report -------------------------------------------------------------------------------------
report = []
for b, a in banks.items():
    base = BASES[b]
    towers = a['towers']
    report.append(f"{b.upper()} WORK RAM 0x{base:08X}: {len(a['roofs'])} roofs, "
                  f"{256 - len(a['roofs'])} guard pages; reached {len(a['reached'] & set(a['roofs']))}; "
                  f"every move both ways: {'yes' if a['two_way'] else 'NO'}")
    for k, t in sorted(towers.items(), key=lambda kv: a['dist'].get(kv[0], 999)):
        if t.hub:
            report.append(f"  {t.hub:13} 0x{base + k[1] * 0x10000 + k[0] * 0x1000:08X}: "
                          f"{'%d towers walked' % a['dist'][k] if k in a['dist'] else 'NOT REACHED'}")
    report.append(f"  dead ends {len(a['dead_ends'])}, T-junctions {len(a['tees'])}, "
                  f"crossroads {len(a['crossroads'])}, loops {a['loops']}; longest lane {a['straight'][0]}")
    if a['sight']:
        r = sorted(a['sight_rule'].values())
        v = sorted(a['sight'].values())
        over = sum(1 for x in v if x > SIGHT_AIM)
        report.append(f"  seen: today's rule median {r[len(r) // 2]}; walls together median {v[len(v) // 2]}, "
                      f"90% {v[int(len(v) * 0.9)]}, worst {v[-1]}, over {SIGHT_AIM}: {over}")

# --- Drawing ------------------------------------------------------------------------------------
# North up. Low Work RAM on the left, High on the right, the pyramid between: as the game has it
# from the corridor, "out" runs to the world's west (corridor.c), so Low is drawn to the west
S = 0.034
XMIN = -(PYRAMID_FOOT + offset(16)) - 600
XMAX = PYRAMID_FOOT + offset(16) + 600
YMIN, YMAX = -BUS_OUT - 900, BUS_OUT + 900
PAD, KEY_W = 40, 520
ox, oy = PAD + KEY_W, PAD + 70
cw = round(ox + (XMAX - XMIN) * S + PAD)
ch_ = round(oy + (YMAX - YMIN) * S + PAD)


def X(x): return round(ox + (XMAX - x) * S, 1)  # Low (x > 0) drawn on the left
def Y(y): return round(oy + (y - YMIN) * S, 1)


def shade(h):
    t = max(0.0, min(1.0, (h + 8 * STEP) / (8 * STEP)))
    lo, hi = (30, 32, 78), (236, 92, 160)
    return '#%02x%02x%02x' % tuple(round(lo[i] + (hi[i] - lo[i]) * t) for i in range(3))


def sight_colour(v):
    return '#5cf28a' if v <= SIGHT_AIM * 3 // 4 else '#ffd23f' if v <= SIGHT_AIM else '#ff4a4a'


svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{cw}" height="{ch_}" viewBox="0 0 {cw} {ch_}" '
       f'font-family="Consolas, monospace">', f'<rect width="{cw}" height="{ch_}" fill="#07070d"/>',
       f'<text x="{PAD}" y="{PAD}" fill="#e8e6f0" font-size="20">Work RAM: the two banks, draft 2 '
       f'(16 x 16 pages of 4 KB each)</text>',
       f'<text x="{PAD}" y="{PAD + 20}" fill="#7a789a" font-size="12">top down, north up. Low Work RAM '
       f'west of the arena, High east; the buses round the pyramid. Each roof: its height, and (small, '
       f'coloured) how many towers are seen from it at most</text>']
# The pyramid: its foot and the arena's walls (its flat top)
svg.append(f'<rect x="{X(PYRAMID_FOOT)}" y="{Y(-PYRAMID_FOOT)}" width="{2 * PYRAMID_FOOT * S}" '
           f'height="{2 * PYRAMID_FOOT * S}" fill="#120a1c" stroke="#4a2a5a"/>')
svg.append(f'<rect x="{X(ARENA_HALF)}" y="{Y(-ARENA_HALF)}" width="{2 * ARENA_HALF * S}" '
           f'height="{2 * ARENA_HALF * S}" fill="#1c1030" stroke="#8a4aa0"/>')
for sx, sy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
    svg.append(f'<line x1="{X(sx * ARENA_HALF)}" y1="{Y(sy * ARENA_HALF)}" x2="{X(sx * PYRAMID_FOOT)}" '
               f'y2="{Y(sy * PYRAMID_FOOT)}" stroke="#4a2a5a"/>')
svg.append(f'<text x="{X(0)}" y="{Y(0)}" fill="#c88ae0" font-size="16" text-anchor="middle">THE ARENA</text>'
           f'<text x="{X(0)}" y="{Y(0) + 18}" fill="#8a6aa0" font-size="11" text-anchor="middle">'
           f'the pyramid: walls 1500 up, its foot 12500 out</text>')
# The corridor: from the arena's wall to Low's way in
svg.append(f'<rect x="{X(PYRAMID_FOOT)}" y="{Y(-SIZE)}" width="{CORRIDOR_LENGTH * S}" height="{2 * SIZE * S}" '
           f'fill="#2a0612" stroke="#c8102e"/><text x="{X(PYRAMID_FOOT - 200)}" y="{Y(-SIZE) - 6}" '
           f'fill="#c8102e" font-size="11">corridor</text>')
# The buses: from each bank's corner by the pyramid, out round it, to the other's
for name, k, sign in (('north', (0, 0), -1), ('south', (0, 14), 1)):
    lt, ht = banks['low']['towers'][k], banks['high']['towers'][k]
    ly = (lt.y0 + lt.y1) / 2
    lx, hx = (lt.x0 + lt.x1) / 2, (ht.x0 + ht.x1) / 2
    by = sign * BUS_OUT
    pts = ' '.join(f'{X(x)},{Y(y)}' for x, y in ((lx, ly), (lx, by), (hx, by), (hx, ly)))
    svg.append(f'<polyline points="{pts}" fill="none" stroke="#0e3a4a" stroke-width="{300 * S + 4}"/>'
               f'<polyline points="{pts}" fill="none" stroke="#38d0ff" stroke-width="2"/>')
    svg.append(f'<text x="{X(0)}" y="{Y(by) + (14 if sign < 0 else -8)}" fill="#38d0ff" font-size="12" '
               f'text-anchor="middle">the {name} data bus (a ride: the other bank loads meanwhile)</text>')
for b, a in banks.items():
    base = BASES[b]
    t0 = a['towers'][(0, 0)]
    left = X(t0.x1) if b == 'low' else X(t0.x1)
    # (High's text clear of the north bus, which comes down beside its column 0)
    title_x = X(a['towers'][(15, 0)].x1) if b == 'low' else X(t0.x1) + 30
    svg.append(f'<text x="{title_x}" y="{Y(t0.y0) - 30}" fill="#e8e6f0" font-size="15">'
               f'{"LOW" if b == "low" else "HIGH"} WORK RAM 0x{base:08X}</text>'
               f'<text x="{title_x}" y="{Y(t0.y0) - 14}" fill="#7a789a" font-size="11">'
               f'{"slow DRAM. Backup RAM, CD block, SCSP" if b == "low" else "fast SDRAM. VDP1, VDP2, video output"}'
               f'</text>')
    # Its ways out, each by its letter on the map and its address here (on the map they collided)
    outs = sorted((t.ch, t.hub, k) for k, t in a['towers'].items() if t.hub)
    for i, (ch, hub, k) in enumerate(outs):
        svg.append(f'<text x="{title_x}" y="{Y(a["towers"][(0, 15)].y1) + 16 + i * 14}" fill="#ffd23f" '
                   f'font-size="11" xml:space="preserve">{ch}  {hub:13} 0x{base + k[1] * 0x10000 + k[0] * 0x1000:08X}'
                   f'  (row {k[1]:X}, column {k[0]:X})</text>')
    for k, ls in a['links'].items():
        ta = a['towers'][k]
        for n, mv in ls:
            if n < k:
                continue
            tb = a['towers'][n]
            colour = '#ff2a55' if mv == 'jump' else '#d8d6e8' if mv in ('drop', 'climb') else '#8a88a8'
            svg.append(f'<line x1="{X((ta.x0 + ta.x1) / 2)}" y1="{Y((ta.y0 + ta.y1) / 2)}" '
                       f'x2="{X((tb.x0 + tb.x1) / 2)}" y2="{Y((tb.y0 + tb.y1) / 2)}" stroke="{colour}" '
                       f'stroke-width="2.5" opacity="0.6"/>')
    for k, t in a['towers'].items():
        x, y, w = X(t.x1) + 1, Y(t.y0) + 1, SIZE * S - 2
        if t.guard:
            svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{w}" fill="#000" stroke="#3a3858"/>')
            continue
        special = t.hub or t.ch in 'Ens'
        stroke = '#ff3b5c' if t.ch == 'E' else '#38d0ff' if t.ch in 'ns' else '#ffd23f' if t.hub else '#000'
        svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{w}" fill="{shade(t.h)}" stroke="{stroke}" '
                   f'stroke-width="{2.5 if special else 1}" opacity="0.92"/>')
        if special:
            label = {'E': 'IN', 'n': 'BUS', 's': 'BUS'}.get(t.ch, t.ch)
            svg.append(f'<text x="{x + w / 2}" y="{y + w / 2 + 3}" fill="{stroke}" font-size="8" '
                       f'font-weight="bold" text-anchor="middle">{label}</text>')
        else:
            svg.append(f'<text x="{x + w / 2}" y="{y + w / 2 - 1}" fill="#f4f2ff" font-size="6" '
                       f'text-anchor="middle">{t.h}</text>')
        if k in a['sight']:
            svg.append(f'<text x="{x + w / 2}" y="{y + w - 2}" fill="{sight_colour(a["sight"][k])}" '
                       f'font-size="6" text-anchor="middle">{a["sight"][k]}</text>')
# Key and the check, on the left
kx, ky = PAD, oy
key = [('#ff2a55', 'jump across a street (level: either way)'),
       ('#d8d6e8', f'step down {STEP} within a block, or climb back up'),
       ('#000', 'guard page: a wall, far above the roofs'),
       ('#ffd23f', 'a way out (hub), at its real address'),
       ('#38d0ff', 'a data bus to the other bank'),
       ('#5cf28a', f'towers seen: under {SIGHT_AIM * 3 // 4}'),
       ('#ffd23f', f'towers seen: up to {SIGHT_AIM} (the aim)'),
       ('#ff4a4a', f'towers seen: over {SIGHT_AIM}')]
svg.append(f'<text x="{kx}" y="{ky}" fill="#e8e6f0" font-size="15">Key</text>')
for i, (colour, text) in enumerate(key):
    y = ky + 22 + i * 19
    svg.append(f'<rect x="{kx}" y="{y - 11}" width="20" height="13" fill="{colour}" stroke="#4a4870"/>'
               f'<text x="{kx + 28}" y="{y}" fill="#c8c6d8" font-size="12">{text}</text>')
y = ky + 22 + len(key) * 19 + 16
svg.append(f'<text x="{kx}" y="{y}" fill="#e8e6f0" font-size="15">The memory map: where the ways out are</text>')
mm = ['0x0018_0000  Backup RAM     Low, row 0 col 4 (just below)', '0x0020_0000  LOW WORK RAM',
      '0x0580_0000  CD block       Low, row E col A (above)', '0x05A0_0000  SCSP           Low, row E col E',
      '0x05C0_0000  VDP1           High, row 0 col 4 (below)', '0x05E0_0000  VDP2           High, row 0 col 8',
      '0x0600_0000  HIGH WORK RAM', '(VDP2 out)   Video output   High, row E col E: the far end']
for i, line in enumerate(mm):
    svg.append(f'<text x="{kx}" y="{y + 18 + i * 14}" fill="#9a98b8" font-size="10.5" '
               f'xml:space="preserve">{line}</text>')
y += 18 + len(mm) * 14 + 16
svg.append(f'<text x="{kx}" y="{y}" fill="#e8e6f0" font-size="15">Check</text>')
for i, line in enumerate(report):
    svg.append(f'<text x="{kx}" y="{y + 18 + i * 14}" fill="#9a98b8" font-size="10" '
               f'xml:space="preserve">{line}</text>')
svg.append('</svg>')

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.normpath(os.path.join(here, '..', 'docs', 'images', 'ram_banks_plan.svg'))
with open(out, 'w', encoding='utf-8') as fh:
    fh.write('\n'.join(svg))
print('\n'.join(report))
print('->', out)

# --- Pictures for a phone (--png) ---------------------------------------------------------------
# python tools/ram_banks_plan.py --png: the whole plan at twice its size, and each bank cut out at
# three times (its towers' numbers readable on a phone), as PNGs beside the SVG. Drawn by Edge or
# Chrome with no window: an HTML page showing just that part of the SVG, at the window's size
if '--png' in sys.argv:
    import subprocess
    import tempfile
    browsers = [r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
                r'C:\Program Files\Google\Chrome\Application\chrome.exe']
    browser = next((b for b in browsers if os.path.exists(b)), None)
    if browser is None:
        raise SystemExit('--png needs Edge or Chrome')
    svg_url = 'file:///' + out.replace(os.sep, '/')

    def shoot(name, left, top, width, height, scale):
        page = os.path.join(tempfile.gettempdir(), 'ram_banks_%s.html' % name)
        with open(page, 'w', encoding='utf-8') as fh:
            fh.write(f'<html><body style="margin:0;overflow:hidden;background:#07070d">'
                     f'<img src="{svg_url}" style="position:absolute;left:{-left}px;top:{-top}px;'
                     f'width:{cw}px;height:{ch_}px"></body></html>')
        png = os.path.join(os.path.dirname(out), 'ram_banks_plan%s.png' % ('' if name == 'all' else '_' + name))
        run = subprocess.run([browser, '--headless=new', '--disable-gpu', '--hide-scrollbars',
                        f'--force-device-scale-factor={scale}', f'--window-size={round(width)},{round(height)}',
                        # (a profile each: one Edge still closing holds its own, and the next draws nothing)
                        '--user-data-dir=' + os.path.join(tempfile.gettempdir(), 'ram_banks_browser_' + name),
                        '--screenshot=' + png, 'file:///' + page.replace(os.sep, '/')],
                       capture_output=True, text=True, timeout=120)
        print('->', png, 'written' if os.path.exists(png) else 'NOT written: ' + run.stderr[-300:] + run.stdout[-300:])

    shoot('all', 0, 0, cw, ch_, 2)
    for b, a in banks.items():
        ts = a['towers'].values()
        left = min(X(t.x1) for t in ts) - 20
        right = max(X(t.x0) for t in ts) + 20
        top = Y(a['towers'][(0, 0)].y0) - 45
        bottom = Y(a['towers'][(0, 15)].y1) + 16 + 3 * 14 + 4
        width = max(right - left, 420)  # room for the ways out's lines
        shoot(b, left, top, width, bottom - top, 3)

# --- The game's table (--c) ---------------------------------------------------------------------
# python tools/ram_banks_plan.py --quick --c: prints Low Work RAM's towers for src/game/city.c, row
# by row (paste it in). The game's heights are the plan's, so the moves checked here are the ones
# the walker meets. Kinds: a guard page; a way out (CITY_HUB: lit); every other a roof
if '--c' in sys.argv:
    low = banks['low']['towers']
    print('// tools/ram_banks_plan.py --c: Low Work RAM, 16 x 16, row by row')
    for row in range(ROWS):
        cells = []
        for col in range(COLUMNS):
            t = low[(col, row)]
            kind = 'CITY_GUARD' if t.guard else 'CITY_HUB' if t.hub else 'CITY_ROOF'
            cells.append('TOWER(%d, %d, %d, %s)' % (col, row, t.h, kind))
        print('\t' + ', '.join(cells) + ', // row %X' % row)
