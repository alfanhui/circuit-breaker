"""
Work RAM's floor plan, first draft (docs/WORK_RAM.md). The city is laid out as RAM is: a grid of
equal blocks, like cells in a spreadsheet. Mirror's Edge is in the art and the movement, not the
layout. The routes come from guard pages: walls of towers too tall to stand on, so the straight
way is rarely the way.

  python tools/work_ram_plan.py   ->  docs/images/work_ram_plan.svg, and a report on stdout

The map below is the level: edit it and run this again. Not used by the game yet. Once it's
agreed, the same map becomes a C table, and the checks here become unit tests.

The rules (WORK_RAM.md, "Moving"; game/walker.h), in world units at the rescaled size (2026-11-02:
the eye 68 up, a unit ~2.3 cm, a grid cell 250):
  - Each map character is one tower, 500 x 500 (11.5 m). Two side by side in one block (2 x 2
    towers) touch: you drop off a lip onto the lower one, step up a kerb (16), or at a full run
    climb up to 100 (2026-11-06: so a drop isn't one way). Up to 120 down is a clean drop;
    further is a long drop (a stagger, and the drone's called).
  - Blocks are apart, across a street: crossing is a jump, at a full run. The landing must be no
    higher than you left and up to 120 below (further below: a long drop at the end of it).
    Streets are 200: a jump clears ~225 on the level (2026-11-06: was 100 and ~140, too small to
    read as a jump from the eye). (The high bank's longer flight isn't built: 200 there too.)
  - A street, or a freed block, is a fall to nothing: a reset.
  - The data bus's bridges join the two banks, across the channel.
  - Each hub has a lift back up to the entry: a safe zone.
  - Heights: the entry's roof is the corridor's floor (0); the city falls away below it, each
    tower lower the further it is from the entry along the way you can walk
    (not as the crow flies), so every route goes down. Where two routes meet, the difference can
    be a long drop: a quicker way down, and a louder one.
"""
import os

# --- The map ------------------------------------------------------------------------------------
# One character a tower. Columns 0-7 the low bank, 8-15 the high; the channel's between them.
#   .  a roof                     #  a guard page: too tall to stand on (walls, cover)
#   (space) a freed block: a fall  =  a roof the data bus leaves from (paired across the channel)
#   E  the entry (the corridor comes in from the west)
#   C B S 1 2 V  the hubs: CD block, Backup RAM, SCSP, VDP1, VDP2, Video output
MAP = [
    # low       high
    '.C..#...' '..#..2..',  # 0  (north)
    '....#.#.' '..#.##..',  # 1
    '.##.#..=' '=.#.....',  # 2
    '###.#...' '..#.#.#.',  # 3
    '..#...#.' '..#.#...',  # 4
    'E.#.###.' '......#S',  # 5
    '..#.....' '####.##.',  # 6
    '..####.#' '...#....',  # 7
    '..#....#' '.#.#.##.',  # 8
    '...#.#.=' '=.#...#.',  # 9
    '.#.#.#..' '.###.#..',  # 10
    '...B.#..' '#V ..#1.',  # 11 (south)
]
HUBS = {  # what opens each way out
    'C': ('CD block', 'open from the start: the first way out'),
    'B': ('Backup RAM', 'CD block: the addresses read, a hidden way in'),
    'S': ('SCSP', 'Backup RAM: a ghost save to draw the drone off its door'),
    '1': ('VDP1', 'SCSP: a voice, to answer the hum'),
    '2': ('VDP2', 'VDP1: a line drawn becomes a bridge'),
    'V': ('Video output', 'VDP2: a longer vault, over a freed block'),
}
GATED = [((11, 11), (9, 11), 'VDP2')]  # moves only open with a skill: from, to (column, row)
PATROLS = [  # the watchdog's routes, as loops of tower corners (column, row), along streets
    [(3, 2), (7, 2), (7, 7), (3, 7)],
    [(8, 3), (14, 3), (14, 8), (8, 8)],
]

QUARTER = 500
LOW_STREET, HIGH_STREET, CHANNEL = 200, 200, 1200
TOP, STEP, LOWEST = 0, 50, -2400
CLEAN_DROP, LONG_DROP_MAX = 120, 600
KERB, CLIMB, LOW_REACH, HIGH_REACH = 16, 100, 225, 225  # walker.h: WALKER_STEP_UP, WALKER_CLIMB, a jump
GUARD_ABOVE, SEALED_ABOVE = 400, 120  # far over a climb (100); over it

# --- Towers -------------------------------------------------------------------------------------


def corner(col, row):
    """A tower's north-west corner, world units (x east, y south)."""
    high = col >= 8
    c = col - 8 if high else col
    street = HIGH_STREET if high else LOW_STREET
    x = c * QUARTER + (c // 2) * street
    if high:
        x += 4 * 2 * QUARTER + 3 * LOW_STREET + CHANNEL
    y = row * QUARTER + (row // 2) * street
    return x, y


def wobble(col, row):
    """A fixed few units either way, so it isn't too neat: -10..10 (a kerb's 16 is the most two
    roofs side by side may differ upwards)."""
    return ((col * 73856093) ^ (row * 19349663)) % 21 - 10


class Tower:
    def __init__(self, col, row, ch):
        self.col, self.row, self.ch = col, row, ch
        self.x0, self.y0 = corner(col, row)
        self.x1, self.y1 = self.x0 + QUARTER, self.y0 + QUARTER
        self.high = col >= 8
        self.guard = ch == '#'
        self.hub = HUBS.get(ch)
        self.lift = self.hub is not None
        self.entry = ch == 'E'
        self.bus = ch == '='
        self.h = 0


towers = {}
for row, line in enumerate(MAP):
    for col, ch in enumerate(line):
        if ch != ' ':
            towers[(col, row)] = Tower(col, row, ch)
entry = next(t for t in towers.values() if t.entry)
bridges = [((7, r), (8, r)) for r, line in enumerate(MAP) if line[7] == '=' and line[8] == '=']

# Heights: walked distance from the entry (towers side by side, and the bridges)


def neighbours(t):
    for dc, dr in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        n = towers.get((t.col + dc, t.row + dr))
        if n and not n.guard and not (t.col == 7 and dc == 1) and not (t.col == 8 and dc == -1):
            yield n
    for a, b in bridges:
        if (t.col, t.row) == a:
            yield towers[b]
        if (t.col, t.row) == b:
            yield towers[a]


dist = {(entry.col, entry.row): 0}
todo = [entry]
while todo:
    t = todo.pop(0)
    for n in neighbours(t):
        k = (n.col, n.row)
        if k not in dist:
            dist[k] = dist[(t.col, t.row)] + 1
            todo.append(n)
for k, t in towers.items():
    if t.guard:
        t.h = 0
    elif k in dist:
        t.h = max(LOWEST, (TOP - STEP * dist[k] + (0 if t.entry else wobble(*k))) // 10 * 10)
    else:
        t.h = LOWEST  # out of reach on foot: a gated hub
for t in towers.values():
    if t.guard:  # far above anything near it
        t.h = max([n.h for n in towers.values() if abs(n.col - t.col) + abs(n.row - t.row) <= 2
                   and not n.guard] + [TOP]) + GUARD_ABOVE

# --- Moves --------------------------------------------------------------------------------------


def overlap(a0, a1, b0, b1):
    return min(a1, b1) - max(a0, b0)


def moves_from(a):
    out = []
    if a.guard:
        return out
    for b in towers.values():
        if b is a or b.guard:
            continue
        dz = b.h - a.h
        for gap, along in ((max(b.x0 - a.x1, a.x0 - b.x1), overlap(a.y0, a.y1, b.y0, b.y1)),
                           (max(b.y0 - a.y1, a.y0 - b.y1), overlap(a.x0, a.x1, b.x0, b.x1))):
            if along < 200:
                continue
            if gap == 0:
                if -CLEAN_DROP <= dz <= KERB:
                    out.append(('drop', b))
                elif KERB < dz <= CLIMB:
                    out.append(('climb', b))
                elif -LONG_DROP_MAX <= dz < -CLEAN_DROP:
                    out.append(('long', b))
            elif 0 < gap <= (HIGH_REACH if a.high else LOW_REACH):
                if -CLEAN_DROP <= dz <= 0:
                    out.append(('vault', b))
                elif -LONG_DROP_MAX <= dz < -CLEAN_DROP:
                    out.append(('longvault', b))
    for p, q in bridges:
        if (a.col, a.row) == p:
            out.append(('bridge', towers[q]))
        if (a.col, a.row) == q:
            out.append(('bridge', towers[p]))
    for f, t, _ in GATED:
        if (a.col, a.row) == f:
            out.append(('gated', towers[t]))
    if a.lift:
        out.append(('lift', entry))
    return out


moves = {k: moves_from(t) for k, t in towers.items()}


def reach(start, kinds):
    seen, todo_ = {start}, [start]
    while todo_:
        k = todo_.pop()
        for kind, b in moves[k]:
            if kind in kinds and (b.col, b.row) not in seen:
                seen.add((b.col, b.row))
                todo_.append((b.col, b.row))
    return seen


def route(start, goal, kinds):
    """The fewest moves from start to goal: the list of towers"""
    back, todo_ = {start: None}, [start]
    while todo_:
        k = todo_.pop(0)
        if k == goal:
            path = []
            while k:
                path.append(k)
                k = back[k]
            return path[::-1]
        for kind, b in moves[k]:
            n = (b.col, b.row)
            if kind in kinds and n not in back:
                back[n] = k
                todo_.append(n)
    return None


OPEN = {'drop', 'climb', 'long', 'vault', 'longvault', 'bridge'}
E = (entry.col, entry.row)
lifts = {k for k, t in towers.items() if t.lift}

# Dead ends: every route only goes down, so the bottom of a branch that doesn't end at a hub is a
# roof you'd be stuck on. Each one's raised out of reach (above everything round it): sealed,
# allocated memory you can't get onto. That can leave the roof that led to it a dead end, so it
# goes round again until every roof you can reach has a way on to a lift. What's left are the
# routes; the sealed roofs are blocks as much as the guard pages are
sealed = set()
while True:
    reachable = reach(E, OPEN | {'gated'})
    stuck = [k for k in reachable if k not in lifts and not (reach(k, OPEN | {'gated'}) & lifts)]
    if not stuck:
        break
    for k in stuck:
        t = towers[k]
        around = [n.h for n in towers.values() if not n.guard and n is not t
                  and abs(n.col - t.col) + abs(n.row - t.row) == 1]
        t.h = max(around + [t.h]) + SEALED_ABOVE
        sealed.add(k)
    moves = {k: moves_from(t) for k, t in towers.items()}
from_entry = reach(E, OPEN | {'lift'})
with_skills = reach(E, OPEN | {'lift', 'gated'})
report, routes = [], {}
for k, t in towers.items():
    if not t.hub:
        continue
    r = route(E, k, OPEN | {'gated'})
    routes[k] = r
    straight = abs(k[0] - E[0]) + abs(k[1] - E[1])
    how = 'reached' if k in from_entry else ('with its skill' if k in with_skills else 'NOT REACHED')
    report.append(f'{t.hub[0]:13} {t.h:5}: {how}, {len(r) - 1 if r else "-"} towers walked '
                  f'({straight} straight)')
standable = [k for k, t in towers.items() if not t.guard]
unreached = [k for k in standable if k not in with_skills and k not in sealed]
stuck = [k for k in with_skills if not (reach(k, OPEN | {'lift', 'gated'}) & lifts)]
report.append(f'{len(standable)} roofs, {len(sealed)} sealed. Not reached: {len(unreached)}. '
              f'Stuck: {len(stuck)}')
counts = {}
for ms in moves.values():
    for kind, _ in ms:
        counts[kind] = counts.get(kind, 0) + 1
report.append('Moves: ' + ', '.join(f'{v} {k}' for k, v in sorted(counts.items())))

# --- Drawing ------------------------------------------------------------------------------------

S = 0.08
W = corner(15, 0)[0] + QUARTER
H = corner(0, 11)[1] + QUARTER
PAD, WEST, KEY_W = 50, 70, 440
cw = round(PAD + WEST + W * S + 30 + KEY_W)
ch_ = round(max(2 * PAD + 70 + H * S, PAD + 70 + 26 + 14 * 23 + 20 + 22 + len(HUBS) * 32 + 12 + 30
                + 9 * 14 + PAD))
ox, oy = PAD + WEST, PAD + 70


def X(v): return round(ox + v * S, 1)
def Y(v): return round(oy + v * S, 1)


def shade(h):
    """Height to a colour: low roofs deep navy, high ones up to the walls' pink (the sky's haze)"""
    t = max(0.0, min(1.0, (h - LOWEST) / (TOP - LOWEST)))
    lo, hi = (30, 32, 78), (236, 92, 160)
    return '#%02x%02x%02x' % tuple(round(lo[i] + (hi[i] - lo[i]) * t) for i in range(3))


svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{cw}" height="{ch_}" '
       f'viewBox="0 0 {cw} {ch_}" font-family="Consolas, monospace">',
       f'<rect width="{cw}" height="{ch_}" fill="#07070d"/>',
       '<defs><marker id="a" viewBox="0 0 8 8" refX="7" refY="4" markerWidth="5" markerHeight="5" '
       'orient="auto"><path d="M0 0 8 4 0 8z" fill="context-stroke"/></marker></defs>',
       f'<text x="{PAD}" y="{PAD}" fill="#e8e6f0" font-size="20">Work RAM: floor plan, draft 4 (wider streets, climbs)'
       f'</text>',
       f'<text x="{PAD}" y="{PAD + 20}" fill="#7a789a" font-size="12">top down, north up. A grid of '
       f'memory, as a spreadsheet; guard pages wall off the straight way. Roofs show their height'
       f'</text>']

lx1, hx0 = X(corner(7, 0)[0] + QUARTER), X(corner(8, 0)[0])
svg.append(f'<text x="{X(0)}" y="{oy - 24}" fill="#9a98b8" font-size="13">LOW WORK RAM 0x0020_0000'
           f'</text><text x="{X(0)}" y="{oy - 10}" fill="#5c5a78" font-size="11">slow DRAM: streets '
           f'200, jumps up to 225, climbs up to 100</text>')
svg.append(f'<text x="{hx0}" y="{oy - 24}" fill="#9a98b8" font-size="13">HIGH WORK RAM 0x0600_0000'
           f'</text><text x="{hx0}" y="{oy - 10}" fill="#5c5a78" font-size="11">fast: streets 200, '
           f'jumps up to 225 (longer: to come)</text>')
cx = (lx1 + hx0) / 2
svg.append(f'<rect x="{lx1}" y="{Y(0)}" width="{hx0 - lx1}" height="{H * S}" fill="#0b0b16"/>')
svg.append(f'<circle cx="{cx}" cy="{Y(H * 0.45)}" r="{450 * S}" fill="#0f0f1c" stroke="#5c5a78" '
           f'stroke-dasharray="4 4"/><text x="{cx}" y="{Y(H * 0.45) + 4}" fill="#8a88a8" '
           f'font-size="10" text-anchor="middle">THE STACK</text>')
# Column letters and row addresses, as a spreadsheet's
for col in range(16):
    x = (X(corner(col, 0)[0]) + X(corner(col, 0)[0] + QUARTER)) / 2
    svg.append(f'<text x="{x}" y="{oy - 40}" fill="#4a4870" font-size="10" text-anchor="middle">'
               f'{col:X}</text>')
for row in range(len(MAP)):
    y = (Y(corner(0, row)[1]) + Y(corner(0, row)[1] + QUARTER)) / 2
    svg.append(f'<text x="{X(0) - 8}" y="{y + 3}" fill="#4a4870" font-size="9" text-anchor="end">'
               f'{row * 0x100:04X}</text>')
# The corridor in
e = entry
svg.append(f'<rect x="{X(e.x0) - 60}" y="{Y(e.y0 + 300)}" width="60" height="{400 * S}" '
           f'fill="#2a0612" stroke="#c8102e"/>')
# Bridges
for p, q in bridges:
    a, b = towers[p], towers[q]
    svg.append(f'<rect x="{X(a.x1)}" y="{Y(a.y0 + 350)}" width="{X(b.x0) - X(a.x1)}" '
               f'height="{300 * S}" fill="#0e3a4a" stroke="#38d0ff"/><text x="{cx}" '
               f'y="{Y(a.y0 + 350) - 4}" fill="#38d0ff" font-size="10" text-anchor="middle">data bus'
               f'</text>')
# Freed blocks
for row, line in enumerate(MAP):
    for col, c in enumerate(line):
        if c == ' ':
            x, y = corner(col, row)
            svg.append(f'<rect x="{X(x) + 3}" y="{Y(y) + 3}" width="{QUARTER * S - 6}" '
                       f'height="{QUARTER * S - 6}" fill="none" stroke="#3a3858" '
                       f'stroke-dasharray="3 3"/>')
# Towers
for t in towers.values():
    x, y, w = X(t.x0) + 1, Y(t.y0) + 1, QUARTER * S - 2
    if t.guard:
        svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{w}" fill="#000" stroke="#5a5880" '
                   f'stroke-width="2"/><line x1="{x}" y1="{y}" x2="{x + w}" y2="{y + w}" '
                   f'stroke="#5a5880"/><line x1="{x + w}" y1="{y}" x2="{x}" y2="{y + w}" '
                   f'stroke="#5a5880"/>')
        continue
    stroke, sw = ('#ff3b5c', 3) if t.entry else (('#ffd23f', 3) if t.lift else ('#000', 1))
    if (t.col, t.row) in sealed:
        svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{w}" fill="#2a2840" stroke="#46446a" '
                   f'stroke-width="1"/><rect x="{x + 5}" y="{y + 5}" width="{w - 10}" height="{w - 10}" '
                   f'fill="none" stroke="#46446a" stroke-dasharray="2 2"/>')
        continue
    svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{w}" fill="{shade(t.h)}" '
               f'stroke="{stroke}" stroke-width="{sw}"/>')
    svg.append(f'<text x="{x + w / 2}" y="{y + w / 2 + 4}" fill="#f4f2ff" font-size="10" '
               f'text-anchor="middle">{t.h}</text>')
    if t.hub or t.entry:
        label = 'ENTRY' if t.entry else t.hub[0]
        col = '#ff3b5c' if t.entry else '#ffd23f'
        svg.append(f'<text x="{x + w / 2}" y="{y + 11}" fill="{col}" font-size="9" '
                   f'font-weight="bold" text-anchor="middle">{label}</text>')
    if (t.col, t.row) not in with_skills:
        svg.append(f'<text x="{x + w / 2}" y="{y + w - 4}" fill="#ff5050" font-size="8" '
                   f'text-anchor="middle">unreached</text>')

# Moves: a short arrow over the side each crosses
style = {'drop': ('#d8d6e8', ''), 'long': ('#ff8c1a', ''), 'vault': ('#ff2a55', ''),
         'longvault': ('#ff8c1a', '4 2'), 'gated': ('#b06cff', '4 2')}
for k, ms in moves.items():
    a = towers[k]
    if k in sealed or k not in with_skills:
        continue  # only the moves you can make: none off a roof you can't get onto
    for kind, b in ms:
        if kind not in style:
            continue
        colr, dash = style[kind]
        ax, ay = (X(a.x0) + X(a.x1)) / 2, (Y(a.y0) + Y(a.y1)) / 2
        bx, by = (X(b.x0) + X(b.x1)) / 2, (Y(b.y0) + Y(b.y1)) / 2
        nx, ny = (by - ay), -(bx - ax)
        n = max(1.0, (nx * nx + ny * ny) ** 0.5)
        f0, f1 = (0.30, 0.70) if kind != 'gated' else (0.15, 0.85)
        sx, sy = ax + (bx - ax) * f0 + nx / n * 3, ay + (by - ay) * f0 + ny / n * 3
        tx, ty = ax + (bx - ax) * f1 + nx / n * 3, ay + (by - ay) * f1 + ny / n * 3
        d = f' stroke-dasharray="{dash}"' if dash else ''
        svg.append(f'<line x1="{sx:.1f}" y1="{sy:.1f}" x2="{tx:.1f}" y2="{ty:.1f}" stroke="{colr}" '
                   f'stroke-width="1.6" marker-end="url(#a)"{d}/>')

# Patrols: along the streets round loops of blocks
for loop in PATROLS:
    pts = []
    for col, row in loop:
        x, y = corner(col, row)
        street = HIGH_STREET if col >= 8 else LOW_STREET
        pts.append(f'{X(x - street / 2)},{Y(y - street / 2)}')
    svg.append(f'<polygon points="{" ".join(pts)}" fill="none" stroke="#7cff6b" stroke-width="2" '
               f'stroke-dasharray="10 6" opacity="0.75"/>')

# Key
kx, ky = X(W) + 30, oy
svg.append(f'<text x="{kx}" y="{ky}" fill="#e8e6f0" font-size="15">Key</text>')
rows = [('roof', shade(500), 'roof: its height (lower is navy)'),
        ('roof', shade(3200), '...higher is pinker'),
        ('arrow', '#d8d6e8', 'drop: step off, up to 120 down'),
        ('arrow', '#ff8c1a', 'long drop: a stagger, calls the drone'),
        ('arrow', '#ff2a55', 'jump: running across a street'),
        ('dash', '#ff8c1a', 'vault ending in a long drop'),
        ('dash', '#b06cff', 'gated: needs a skill'),
        ('guard', '#5a5880', 'guard page: too tall to stand on, a wall'),
        ('sealed', '#46446a', 'sealed: higher than all round, out of reach'),
        ('freed', '#3a3858', 'freed block: hollow, a fall'),
        ('lift', '#ffd23f', 'hub: a way out, and a lift up'),
        ('bus', '#38d0ff', 'data bus: the bridge between banks'),
        ('patrol', '#7cff6b', "the watchdog's patrol")]
for i, (kind, colr, text) in enumerate(rows):
    y = ky + 26 + i * 23
    if kind == 'roof':
        svg.append(f'<rect x="{kx}" y="{y - 12}" width="24" height="16" fill="{colr}"/>')
    elif kind in ('arrow', 'dash'):
        d = ' stroke-dasharray="4 2"' if kind == 'dash' else ''
        svg.append(f'<line x1="{kx}" y1="{y - 4}" x2="{kx + 24}" y2="{y - 4}" stroke="{colr}" '
                   f'stroke-width="2" marker-end="url(#a)"{d}/>')
    elif kind == 'guard':
        svg.append(f'<rect x="{kx}" y="{y - 12}" width="24" height="16" fill="#000" stroke="{colr}" '
                   f'stroke-width="2"/>')
    elif kind == 'sealed':
        svg.append(f'<rect x="{kx}" y="{y - 12}" width="24" height="16" fill="#2a2840" stroke="{colr}" '
                   f'stroke-dasharray="2 2"/>')
    elif kind == 'freed':
        svg.append(f'<rect x="{kx}" y="{y - 12}" width="24" height="16" fill="none" stroke="{colr}" '
                   f'stroke-dasharray="3 3"/>')
    elif kind == 'lift':
        svg.append(f'<rect x="{kx}" y="{y - 12}" width="24" height="16" fill="{shade(900)}" '
                   f'stroke="{colr}" stroke-width="3"/>')
    elif kind == 'bus':
        svg.append(f'<rect x="{kx}" y="{y - 9}" width="24" height="10" fill="#0e3a4a" stroke="{colr}"/>')
    else:
        svg.append(f'<line x1="{kx}" y1="{y - 4}" x2="{kx + 24}" y2="{y - 4}" stroke="{colr}" '
                   f'stroke-width="2" stroke-dasharray="6 3"/>')
    svg.append(f'<text x="{kx + 32}" y="{y}" fill="#c8c6d8" font-size="12">{text}</text>')

y = ky + 26 + len(rows) * 23 + 20
svg.append(f'<text x="{kx}" y="{y}" fill="#e8e6f0" font-size="15">Ways out: what opens each</text>')
for i, (name, needs) in enumerate(HUBS.values()):
    svg.append(f'<text x="{kx}" y="{y + 22 + i * 32}" fill="#ffd23f" font-size="12">{name}</text>')
    svg.append(f'<text x="{kx}" y="{y + 36 + i * 32}" fill="#9a98b8" font-size="10">{needs}</text>')
y += 22 + len(HUBS) * 32 + 12
svg.append(f'<text x="{kx}" y="{y}" fill="#e8e6f0" font-size="15">Check: the shortest way to each'
           f'</text>')
for i, line in enumerate(report):
    svg.append(f'<text x="{kx}" y="{y + 18 + i * 14}" fill="#9a98b8" font-size="10">{line}</text>')
svg.append(f'<text x="{X(0)}" y="{Y(H) + 22}" fill="#5c5a78" font-size="11">scale: a tower 1000 x '
           f'500 units (2 grid cells a side); the arena is 16000 across</text>')
svg.append('</svg>')

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.normpath(os.path.join(here, '..', 'docs', 'images', 'work_ram_plan.svg'))
with open(out, 'w', encoding='utf-8') as fh:
    fh.write('\n'.join(svg))
print('\n'.join(report))
print('->', out)

# --- The game's table ---------------------------------------------------------------------------
# python tools/work_ram_plan.py --c   prints src/game/city.c's table for the towers built so far
# (COLUMNS x ROWS from the plan's north-west corner), row by row: paste it in. The game's heights
# are the plan's, so the moves this script checks are the moves the walker meets
import sys
if '--c' in sys.argv:
    COLUMNS, ROWS = 7, 12
    print('// tools/work_ram_plan.py --c: columns 0-%d, rows 0-%d, row by row' % (COLUMNS - 1, ROWS - 1))
    for row in range(ROWS):
        cells = []
        for col in range(COLUMNS):
            t = towers.get((col, row))
            if t is None:
                raise SystemExit('a freed block in the built part: the table has no hole for it yet')
            kind = ('CITY_GUARD' if t.guard else 'CITY_HUB' if t.hub else
                    'CITY_SEALED' if (col, row) in sealed else 'CITY_ROOF')
            cells.append('TOWER(%d, %d, %d, %s)' % (col, row, t.h, kind))
        print('\t' + ', '.join(cells) + ', // row %d%s' % (row, ' (the entry)' if row == entry.row else ''))
    # And the route to the first hub, for the unit test that walks it
    r = routes[[k for k, t in towers.items() if t.ch == 'C'][0]]
    print('// the route to the CD block: ' + ', '.join('{%d, %d}' % k for k in r))
