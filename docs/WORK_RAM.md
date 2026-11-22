# Work RAM: design

The plan for level 2, the hub (STORY.md). Decided so far, 2026-11-02:

- Stealth first person, on foot (`game/walker.h`'s controls). No button for interacting: you
  get through by moving and looking.
- The watchdog is a drone that rides up and down between the memory slots. Its look and
  behaviour take after *Arc Raiders*' machines.
- The layout takes after *Mirror's Edge*: rooftops, drops and routes. The skyscrapers are the
  memory blocks.
- **You get from tower to tower by going down.** You step off onto lower roofs. Running at full
  speed at an edge with a roof in reach vaults you across the gap automatically.
- **The player chooses where to go.** Work RAM reaches every other area, but each one is blocked
  until you have the skill to get to it or have unlocked it (below: the way out).
- **Each hub has a lift** back up. Lifts are safe zones: the drone can't get you there.
- **A long drop staggers you.** It's loud, and it pulls the drone in towards you: unauthorised
  direct access. It isn't a reset.

## The way out: a choice, gated

Work RAM is the hub (STORY.md): every other area opens off it, and you choose which to go for.
It works like a Metroidvania, where you can see a place before you can reach it.

- **Each area's way in is blocked** until you can reach it. The block is part of the city:
  - out of reach: across a gap too wide to vault, or up where no drop leads;
  - locked: a sealed door, a dark bus.
- **What you bring back from one area opens the next.** These are proposals to choose between,
  each from what that place gives Shh (STORY.md's table):
  - **CD block** (knowing where it is): the addresses can be read, a map. A route hidden in the
    numbers opens.
  - **SCSP** (a voice): the hum can be answered. A sound that pulls the drone away from you.
  - **Backup RAM** (people outside): **the ghost save** (chosen, below).
  - **VDP1** (being drawn): the trail comes back. A line that becomes a bridge.
  - **VDP2** (how big the world is): longer vaults and drops. The far side of the city.
- **Seen first, reached later.** The way into a blocked area is in sight from a roof you can
  reach, so you know what you're coming back for.

### The ghost save (from Backup RAM)

A save is a state kept for later. Here it's yours: you see your own earlier self replay what you
did, a ghost of you walking your old route.

- **It's a decoy.** The drone sees and hears the ghost as it would you. When the ghost runs,
  lands or walks into the scan, the drone reacts to it and is pulled away from you. You use your
  own past to draw the watchdog off.
- **The order is the puzzle.** Lay a route in which your ghost walks into the drone's sight at
  the right moment, then go the other way while it chases.
- **It fits the story.** Backup RAM is where Shh learns somebody played here before. The ghost is
  that, turned on yourself: a record of play, still moving.
- **On the Saturn, it's cheap:**
  - Record the pad, not the positions. The walker is deterministic, pure C with no randomness,
    so the same buttons from the same start walk the same route exactly.
  - One frame's pad is 2 bytes, so 30 seconds is 3.6 KB.
  - The ghost is a second `walker` fed from the recording, drawn as a flickering glassy
    silhouette.
- **The lifts save (decided).** No button is needed:
  - **Leaving a lift starts recording.** It records until you're back in a lift, or for 30
    seconds at most.
  - **Leaving that lift again replays the last route recorded from it.** Your ghost steps out
    beside you and does what you did last time, while a new recording of this time starts.
  - So you plan a ghost by playing: go out, run past the drone's floor on purpose, come back.
    The next time out, the ghost does that again while you go another way.
  - The lifts are also the safe zones and checkpoints, so the ghost is set up where you're safe.
- **Only after Backup RAM.** Until then the lifts don't save: the skill is the ghost.
- **Memory:** one recording for each lift, 3.6 KB each. Or only the last lift's, if memory's
  tight: you'd go back to the lift you just used.
- **A ghost can't be caught.** The drone chases it, and when it reaches the ghost, the ghost
  breaks up like a corrupted save. Or it just plays to its end and fades.

## Hubs and lifts

The descent goes one way, but a hub you come back through needs a way back up.

- **Each hub has a lift.** A hub is a cluster of towers around one area's way in. The lift
  carries you back up to the top.
- **Lifts are safe zones.** Inside, the drone can't see you and doesn't follow. They're
  checkpoints too: the watchdog is fed there. They're also where you breathe between drops.
- **A lift is a landmark:** lit up the full height of its tower, so you can find your way back.

Everything below is a proposal, to be changed by playing it.

## The city

**The layout is a grid, like RAM (decided).** Equal blocks in rows and columns, like a
spreadsheet. Mirror's Edge is in the art and the movement, not the layout. The routes come from
blocks in the way:

- **Guard pages:** towers far too tall to stand on, used as walls and cover.
- **Sealed blocks:** roofs higher than everything round them, so out of reach.
- **Freed blocks:** hollow, a fall.

The floor plan's draft is `docs/images/work_ram_plan.svg`, drawn by `tools/work_ram_plan.py` from
a text map, one character per tower. Draft 3 (2026-11-03) is at the rescaled size: towers 500
across (11.5 m), streets 100, a jump reaching 140, a clean drop 120, each tower 50 lower than the
last walked, the entry's roof level with the corridor's floor (0) and the city below it, to -2400. Edit the map and run the script again. The script works out
the heights (lower the further you've walked from the entry) and every move by the rules below.
It checks that every hub is reached and that no roof you can get onto is a dead end: a dead end
is sealed instead.

The Saturn's real Work RAM, so the theme is more than paint:

| Real | In the city |
|---|---|
| Two 1 MB banks: low (slow DRAM), high (fast SDRAM) | Two districts. Low: older, packed close, darker; it drags you (slower walking). High: sleek, tall, wide gaps; you fly (faster running, longer vaults). STORY.md already has the drag and the flight |
| Allocated blocks | Lit towers you can stand on |
| Freed blocks | Dark hollow shells: the gaps to get across, or fall through |
| Fragmentation | The level design: how far apart the roofs are |
| Addresses | Floors, numbered in hex on the towers (`0x0604_4C00`). An objective and a sign: "get to 0x0600_0000" |
| The stack, growing down | One vast tower hanging from the top of the address space, growing downwards through the level: a clock you can see |
| The data bus | Bridges between towers that pulses of data run along: routes with timing |
| Memory in use (STORY.md: the machine, live) | The city's density: the more the game is using, the more towers are lit |

**Built (2026-11-03): the entry blocks** (`game/city.h`, 21 towers: the plan's columns 0-2, rows
3-9). The corridor's far end is open on its right half, onto the entry roof. From there: step off
east 50 onto the next tower, or run and jump the street south onto the next block (60 down), and
on down through it to a third. The guard pages stand 400 over the entry, as walls; the sealed roofs
beside it are just out of reach. Drawn as the grey box's boxes, standing on the ground at -3000
(the VDP2 grid moves down to it on foot); each top darker the lower it is. From inside the
corridor the towers are cut to what's seen through the doorway; out in the city, the corridor's
insides aren't drawn (seen back through the doorway, they're missing: still to do). Filmed:
`tests/e2e/city.steps`.

**Built (2026-11-09): the way to the CD block** (`game/city.h`, 84 towers: the plan's columns 0-6,
rows 0-11). The table is the plan's own (`python tools/work_ram_plan.py --c` prints it), so the
heights in the game are the heights the plan checks. The way down winds 28 towers: south through
the entry's blocks, past Backup RAM's hub (its way in gated, for later), east and up column 6, west
along row 6, then north up column 3, a canyon between guard pages, and west along row 0 to the CD
block's hub at -1400. Both hubs are lit cyan, and are targets you can lock onto. A unit test walks
the route against the table's towers: every step a drop, a climb or a street jump the walker makes.
Drawing 84 towers: the sides end 900 below the eye instead of at the ground (below, the black
falls away), and the coarse cull groups the city by 2 x 2 blocks. From the doorway: 60 fps, cpu
55-79%. Deep in, on (3, 6): 55-60 fps, cpu ~100%, VDP1 at its limit when a guard page fills the
screen. Guard pages as occluders were tried and cost more than they saved (8 faces' planes a frame).
Still to do: the hubs' lifts and their ways in, the drone, a film down the whole route.

**Built (2026-11-12): the CD block's lift, the disc and its drive.** Over the city, 4000 up, a
disc 7000 across lies in the sky (an octagon ring in dull slates, the sector at the laser paler,
stepping round): seen from the city and from the arena, before you know what it is. Under it the
drive's works: the spindle, a rail, and on the rail over the CD block's hub the laser's pickup,
a red beam from it up to the disc. A beam of light runs from the hub's roof up to the pickup's
underside, the hatch. Stand on the pad at the roof's middle for three quarters of a second and
the lift takes you up it (`game/lift.h`: no button, 8.5 s, eased) to the hatch, holds, and brings
you back: the level through the hatch isn't built. On the way up the other places come into
sight over the towers (`game/landmarks.h`: one cutout on the horizon for each place in the story). Down at the hub nearly every tower stands
over you, and VDP1 couldn't fill them all: towers wholly behind a nearer, taller one aren't drawn
now (`game/skyline.h`), and far sides end where the screen does.

**Built (2026-11-12): the city all round.** Work RAM has no landmark of its own on the horizon:
it *is* the horizon. A ring of skyline stands round you every way you look (level.c,
`skyline_add`; `tools/make_skyline.py`): near-black monoliths with sharp tops, three rows deep in
one picture, a thin rim or trace of cyan light on a few, the far row fading into violet haze; no
windows (a first pass with warm lit windows read as a cartoon of an old European city). It
follows you three quarters of the way, so it seems four times as far as it is and slides against
the towers and landmarks as you move: parallax, as in Akira's Neo-Tokyo. (Three rings at three
depths were tried first: three times the polygons for little the eye could tell.) In the arena it
stands on the floor, black at its foot; in the city it's nearer and towers over you. The other
landmarks are drawn nearer and bigger than they are in the city (the user's trick): they only
change while they're out of sight.

## A bigger city: both banks of Work RAM, a page a tower (proposal, 2026-11-15)

The ask (the user, 2026-11-15): the city at least 3 times as big, a level in itself, its size
after Work RAM's. Its ways full of dead ends, more than one route, T-junctions and crossroads, so
you can run past the watchdog, outrun it and hide. Start in the middle, where the crash puts you.
Laid out so that what's in the way blocks the view enough to keep 60 fps.

### The size: 256 towers, one megabyte

- **Low Work RAM is 1 MB at `0x0020_0000`.** 16 x 16 towers of 4 KB each (`0x1000`) is 256
  towers, exactly 1 MB.
  - The tower at row r, column c is `0x0020_0000 + r x 0x1_0000 + c x 0x1000`.
  - The debug line's `at 0650` becomes `at 0026_5000`: the same row and column, as an address.
  - The towers' faces could carry their addresses (the art pass).
- **About 3 times today's size:**
  - 256 towers against 84 is 3.05 times.
  - The ground is 9,600 square (16 towers and 8 streets each way), against 4,100 x 7,000: 3.2 times.
- **Guard pages become real.** An operating system leaves a 4 KB page unmapped beside a stack or
  buffer, so a program that runs over it is caught. The city's walls are those pages, 4 KB each.
- **High Work RAM too** (the user's review of draft 1). It's 1 MB at `0x0600_0000`, the same
  16 x 16: the second district, across the arena from Low, reached by the data buses. 512 towers in
  all, six times today's city.

### Starting in the middle, of the west edge

- **The corridor opens at the middle of the west edge** (column 0, row 8: `0x0028_0000`). The
  first proposal was the district's very middle, but the arena's pyramid is in the way (Decided,
  below).
- **Every way leads somewhere.** The routes fan out north, south and east. The six ways out are
  three in each bank, where the Saturn has them round Work RAM (draft 2, below).
- **Heights fall away in rings** from the entry, to -720 at the far corners. The rule is in
  Decided, below.

### The routes: lanes, junctions and loops

- **The roofs you can reach are lanes,** a block wide (2 towers), walled by guard pages taller than
  you. Where lanes meet they make the shapes asked for:
  - a T-junction;
  - a crossroads (four lanes meeting at a street corner, jumped across);
  - a dead end;
  - a loop.
- **Loops are how you outrun the drone.** Go round a block and it loses you behind the walls.
  - The plan tool checks there are several loops near the start, and on the way to each hub.
  - Round a loop, any height lost at a drop or a jump down is won back by climbs within blocks.
- **Dead ends are allowed again.** Until now the tool sealed every dead end. Now a dead end is a
  place to hide: a recess out of the drone's sight, at the price of no way on if it finds you.
- **Hiding:** out of the drone's scan, behind a wall, or in a recess. The drone's patrols run along
  the lanes, so a lane next to one is where it doesn't look.

### Seeing less: the level built to block the view

Today's city shows too much: from a roof patch you see a median of 54 of the 84 towers, and from
some all 84 (measured from `city_seen.c`). A city that falls away below you shows you all of it.
Three times that would be three times VDP1's work. So:

- **The lanes are walled.** Guard pages stand above the roofs beside a lane, so you see along your
  lane to its next turn, and over the walls only the skyline ring and the landmarks.
- **No long straight views.** A lane turns or tees within about 3 blocks (LEVELS.md: break long
  canyons). That's also what makes it a maze.
- **Measured on paper, before it's built.** The plan tool runs `make_city_seen.py`'s rule on the
  plan, and reports for every place you can stand how many towers can be seen from it.
  - The aim is 40 at most anywhere; the tool lists the worst places, to change the map there.
  - A view that's too open is fixed in the plan, not in the game's code.
- **Then the game's own culling** (LEVELS.md's order) on what's left: the frustum, the hiders,
  the floor rule, and the draw budget last.

### Memory and code: what has to change

| What | Today (84) | At 256 | How |
|---|---|---|---|
| The towers' table | 1 KB | 3 KB | as it is |
| What each place sees (`city_seen`) | 3.7 KB | 32 KB per tower: too much | by block instead (64 blocks: 8 bytes a patch): 8 KB, then each tower in a seen block by the hiders, as now |
| The grey boxes | ~3 KB | ~9 KB | indices of 1 byte become 2 (they stop at 255) |
| The walker's roofs | a list of 128 at most, searched | 256+ | found by the grid: the roof under a point from its column and row, not a search |
| The drone's sight | tested against all 84 boxes | 256 | a walk along the grid cells between the drone and you |

All fits Work RAM. The per-frame work is set by what's seen, not by how big the city is.

### Decided (the user, 2026-11-15)

- **Low Work RAM now:** 256 towers. High Work RAM later, as a second district.
- **Heights in rings, nothing one way.** Every street is level, so you can jump it either way.
  Each block steps down 90 on its side away from the entry: a clean drop out, a climb back in. So
  every move works both ways, and a loop can be run round and round. The edge is at -720.
- **The entry in the middle of the west edge**, not the city's middle. The arena's pyramid falls to
  the ground exactly where the corridor ends (its 45-degree slope from the walls' top, 1500, to
  the ground, -3000, is 4500 out: the corridor's length), so the city can't reach back round the
  corridor without towers standing through the pyramid. The corridor opens at column 0, row 8,
  and the routes fan out north, south and east.
- **The plan first.**

### Draft 2: both banks, the ways out by address (2026-11-15)

The user's review of draft 1: all six ways out were in Low Work RAM; plan High too, three in each,
placed roughly as the real Saturn has them round Work RAM; the arena's pyramid in the middle, with
two data buses north and south of it.

`tools/ram_banks_plan.py` draws `docs/images/ram_banks_plan.svg`, both banks in one picture.

- **Where everything is:** Low Work RAM on one side of the arena, High on the other, each with its
  column 0 at the pyramid's foot. The corridor opens onto Low's middle, as before.
- **The buses:** two, north and south of the pyramid. Each runs from Low's corner by the pyramid
  (column 0, rows 0 and E), out round the pyramid (12,500 from the middle), to High's.
  - They're long, about 30,000: the rides the highway proposal (below) wanted, with the other bank
    loading meanwhile.
  - High's heights fall from both buses' ends, so every move there works both ways too.
- **The ways out, by address.** A bank's addresses run row by row (64 KB a row, 4 KB a tower along
  it). So a place whose address is below a bank's sits by its first page (row 0, low columns), and
  one above it by its last (row E, high columns), in address order:

| Address | Place | Bank, tower | Why there |
|---|---|---|---|
| `0x0018_0000` | Backup RAM | Low, row 0, column 4 (`0x0020_4000`) | just below Low Work RAM |
| `0x0020_0000` | Low Work RAM | | |
| `0x0580_0000` | CD block (A-bus) | Low, row E, column A (`0x002E_A000`) | above Low |
| `0x05A0_0000` | SCSP (B-bus) | Low, row E, column E (`0x002E_E000`) | above the CD block |
| `0x05C0_0000` | VDP1 (B-bus) | High, row 0, column 4 (`0x0600_4000`) | just below High Work RAM |
| `0x05E0_0000` | VDP2 (B-bus) | High, row 0, column 8 (`0x0600_8000`) | above VDP1 |
| `0x0600_0000` | High Work RAM | | |
| (VDP2's output) | Video output | High, row E, column E (`0x060E_E000`) | the end of the line: the far corner |

  Split at the middle of the number line, that's the story's order as well: the CD block, Backup
  RAM and SCSP in Low; VDP1, VDP2 and the video output in High, after a bus ride.

| Check | Low | High |
|---|---|---|
| Roofs / guard pages | 135 / 121 | 135 / 121 |
| Reached, every move both ways | all, yes | all, yes |
| Ways out, towers walked | CD block 16, SCSP 24, Backup RAM 56 (from the corridor; the north bus lands beside it) | VDP1 40, VDP2 40, Video output 56 (from the north bus) |
| Dead ends, T-junctions, crossroads, loops | 7, 17, 3, 8 | 5, 16, 3, 8 |
| Longest straight lane | 9 | 9 |
| Towers seen, walls together: median, worst, roofs over 40 | 32, 53, 22 | 31, 58, 23 |

- Low's maze is draft 1's. High's is the generator's seed 54, picked from 120 for the shortest
  lanes and the most dead ends.
- **Still as draft 1 found:** the table that says what's seen has to be made with the walls working
  together (by today's rule each bank would show ~140 towers from the median roof). Long lanes are
  the worst views, and dead ends are few: both are hand edits to the maps.

### Built: Low Work RAM (2026-11-16)

The user: "Execute the lower working ram level. Do not place the watchdog, as I will produce
coordinate routes for these watchdog processes." The 84-tower city is replaced by draft 2's Low
Work RAM, as the plan has it.

- **The towers:** `game/city.c`'s table is the plan's (`python tools/ram_banks_plan.py --quick
  --c`), 16 x 16, row 8 on the corridor's middle line.
  - The debug line's `at` shows the page you stand on, e.g. `at 0028_0000` on the entry.
  - The films' `boot_at` codes stay row << 8 | column << 4: `0800` is the entry, `0EA0` the CD
    block's hub.
- **The ways out:**
  - **The CD block** (row E, column A): the hub, its lift and the disc. The disc now hangs north of
    the hub, over the city, since the hub is on the south edge.
  - **Backup RAM** (row 0, column 4): the A-bus leaves from it, past its gap.
  - **The SCSP** (row E, column E): lit, and a target, like the other hubs.
  - **The landmarks** stand where the addresses say (`game/landmarks.c`): Backup RAM's beyond the
    north edge, the SCSP's beyond the south-east corner, High's three across the arena.
- **No watchdog.** `city.h`'s `city_drone_place` is the format for your routes:
  - each stop is `{out, across, height, CITY_FACES_..., wait}`, in the city's own frame;
  - `city.h` gives the sums from a tower's column and row to its out and across;
  - set `CITY_DRONE_STOPS` to the count.

  Until then the drone isn't flown, drawn or looked for. Its tower boxes are still made (the debug
  line's `at` uses them).
- **Tables widened:** the walker's roofs (300), the grey boxes' groups (two bytes), the seen table
  (32 KB).
- **Tests:**
  - Every street is level and every step in a block a climb or less.
  - The walker runs the route to each of the three hubs, and back, one move at a time.
  - The seen table matches the game's rule (every seventh roof, in full).
- **Measured** (Mednafen, films `low_ram_first`, `low_ram_turn_hub`, `low_ram_turn_mid`, and the
  benchmark with Low Work RAM's 12 views):
  - 9 views pass.
  - "mid back" (47 late frames, cpu 133%), "hub q0" (57, 137%) and "scsp" (22, 122%) don't: the
    CPU, not VDP1.
  - Turning at the CD block's hub, VDP1 missed 25 frames.
- **Why, and what's next:** the seen table by today's rule leaves 161 towers seen on average. The
  table made with the walls working together (median 32 by the plan) is the next step
  (BACKLOG, "Low Work RAM's seen table").

### Draft 1 on paper (2026-11-15)

Low Work RAM alone (its tool, since replaced by draft 2's). The map is a maze on the towers at
even columns and rows: the towers between two of them are a roof where the maze joins them, a
guard page where it doesn't. Made by the tool's generator (seed 12, turning preferred to going
straight, 8 walls opened for loops), then the hubs put in the story's order by walking distance.
It's for editing by hand: one character a tower.

| Check | Draft 1 |
|---|---|
| Roofs / guard pages | 135 / 121 |
| Reached from the entry, every move both ways | all 135, yes |
| Hubs, towers walked | CD block 16, Backup RAM 24, VDP1 34, SCSP 34, VDP2 66, Video output 74 |
| Dead ends (besides the 6 hubs), T-junctions, crossroads, loops | 2, 17, 3, 8 |
| Longest straight lane | 9 towers (row 8, east of the entry) |
| Towers seen, by today's table rule | median 140, worst 168 of 256 |
| Towers seen, the walls together (sight lines) | median 32, 90% 43, worst 53; 22 roofs over 40 |

**What it found:**

- **Today's way of making the table can't hold this city.** `make_city_seen.py` calls a tower
  hidden only if one hider covers all of it. A lane's wall is many guard pages, broken by a street
  every two towers: no one of them covers a tower behind, so by that rule you'd see 140 of the 256
  from the median roof. Together the walls hide most of it: 32.
  - So the table has to be made with the walls working together, and still be sure, never
    hiding a sliver that shows. That's step 3's hard part (below).
  - The tool's sight lines are sampled: an estimate for the plan, not a table for the game.
- **The worst views are the long lanes:** row 8 east of the entry, row E, column 4. Breaking them
  with a turn or a wall brings those roofs under 40.
- **Few dead ends:** the loops opened most of them. Fewer loops, or spurs added by hand, if more
  hiding places are wanted.

### Build order

1. **On paper.** `tools/ram_banks_plan.py` (new; `work_ram_plan.py` stays the first city's), and
   its checks:
   - every hub reached;
   - loops near the start;
   - dead ends allowed;
   - the sight count for every patch.

   An SVG to agree on. No game code.
2. **The game's tables widened**, and the grid lookups (walker, drone, address), still with
   today's 84 towers: the tests and films must come out the same.
3. **The district in.** The 256 towers, and `city_seen` made a new way, by blocks and with the
   walls working together, conservatively.
   - One idea to make that sure: for a patch and a tower, the walls hide it if every line from the
     patch to the tower crosses one. Lines in the plan, and towers as rectangles, so this can be
     worked out exactly with intervals of angle and height, not samples.

   Then the benchmark's views and running films, with `px` under 350k.
4. **The drone's patrols on the new lanes:** a chase, and losing it.
5. **The hubs on the edges.** The CD block's lift and disc move with its hub, Backup RAM's too.
   Today's route test (`test_city.c`) and the city's films are replaced.

## Crossing the city: the highways, and hiding the loads (proposal, 2026-11-12)

The city can't all be in memory at once: the Saturn has 2 MB of Work RAM, and the city slice
already built (84 towers, the corridor) is one district of many. So the city is **districts**, and
between two districts is a **highway**: the ride across is when the next district comes off the
disc. No loading screen, as long as the ride is longer than the load.

- **What a highway is, in lore: a bus.** The SH-2s and the other chips talk over buses; data
  crosses the machine on them. A highway is a bus line between two memory regions: a long,
  straight, elevated lane of light over the dark between districts, the skyline sliding past on
  both sides (the three rings: the parallax does the work of speed).
- **How you ride it (the choice to make):**
  1. *Your cycle comes back.* Shh is a process; on a bus it's data in transit, and it moves the
     way it did in the arena. The racing controls return for the ride, a trail behind you, the
     Watchdog's drones chasing along the bus. The game's two halves meet, and the ride is play,
     not a wait.
  2. *A transfer you stand on,* like the CD block's lift (`game/lift.h`): a packet carrier that
     takes you along the bus while you turn and look. Simpler, a breather between districts.
  3. Both: carriers on the minor buses, the cycle on the main bus.
- **The numbers:** the CD reads ~300 KB a second (double speed), plus a seek of up to ~0.3 s. A
  district (its tower table, textures, sounds) at 150-300 KB loads in about a second; a ride of
  6-10 s covers it with room to spare, even with music streaming. jo can read a file in the
  background (`JO_MAX_FS_BACKGROUND_JOBS`, set to 1 in the makefile).
- **If the disc is late (a scratched disc, an emulator): the loading screen is in the story.** The
  bus goes into a wait state: the picture freezes and the code screen takes over (the terminal
  we already have: green text on black, "DMA transfer to Work RAM-H ...") until the load's done.
  The breach's hidden load (input.c, `hidden_load`) is the same thing done once already.
- **Memory:** one district in Work RAM at a time, the next loading into a second buffer during
  the ride (two districts' worth of the free zone), the one behind you freed as you arrive.

**The way in:** the corridor's far end opens onto a ledge. You walk out onto a rooftop and the
city falls away below you, further than the arena was across: Mirror's Edge's first rooftop.

**The look:** near-black glass towers. The way on is marked in the corridor's pulsing blood-red,
like Mirror's Edge's red runner vision: a lip you can drop from, a roof you can reach. Below,
the city fades into fog over a grid far down (VDP2), so the drop feels endless.

## Moving: walk, run, drop, vault

The pad has no button left, so speed does the work: how long you've held forward.

| Move | How | Why |
|---|---|---|
| Creep | Tapping forward: the speed never ramps up | Silent. The ramp we already have becomes the stealth control |
| Walk | Holding forward: 3 a frame, as now | Quiet |
| Run | Holding forward straight on for ~0.8 s ramps up to 6 a frame. Turning hard (a turn held to its full speed: past ~15 degrees), strafing, backing off, or running into a wall or lip head on drops you back to a walk. A tap of a turn steers without losing it | Loud. You commit to it, and it's how you vault |
| Stop at a lip | Walking into a roof's edge stops you there, as a wall does | You never walk off a roof by accident |
| Drop | Keep pushing against the lip for ~0.3 s and you step off. Your head looks down at the drop as you push, so you see what you're dropping into | You meant it |
| Jump | At a full run, a lip with a clean landing in reach: you jump off it (45 up: about your waist) and fly on at your speed, across a street (200) or down onto a lower roof. The whole flight's worked out as you leave | Mirror's Edge's flow, with no jump button. 18 up over 100-wide streets didn't read as a jump from the eye |
| Vault over | At a full run, something no higher than your waist and thin enough to clear (a rail, a crate): hands on its near edge, a small hop, and you land beyond it, still running | Flown first, as a jump: too deep to clear, it's a vault up instead |
| Vault up | At a full run, a wall no higher than your waist (48), too deep to vault over: you're up onto it in a stride, the eye following | |
| Climb | At a full run, a ledge up to 100 (2.3 m, a ledge your hands reach): you climb up onto it. The eye follows at half a step's pace, and you're down to a walk on top. Higher is a wall | A drop within a block isn't one way: you can climb back. Across a street you can't |
| Slide | At a full run, a bar you'd hit your head on with room under it (its bottom 34-76 up: a duct, a pipe): you slide under it, the eye low (26), 0.6 s, slowing to a walk, the pad not steering. Only if you'll come out with room to stand | No crouch button: the run does it, as everything else |
| Running at an edge with nothing clean in reach | You stop at the lip, the run lost (a long drop below: push on to step off, as walking) | Never killed, or staggered, for running |

Built (2026-11-03, `game/walker.h`): the run, the jump off lips and across gaps, the vault up,
and the eye easing up and down steps (the feet snap, the view glides over a few frames).
2026-11-06: the climb, the vault over and the slide; jumps 45 up (were 18), clearing ~225 on the
level; the city's streets 200 (were 100). A landing's drop counts from where you left, so a
jump's hop never makes a clean drop a long one. Roofs can have a bottom (a bar, to slide under).
The grey box has a yard on the corridor's right half to try them (a rail, a duct, a ledge of 90
beside one of 140) and the crate before the stairs, now vaulted over. Later, hands and arms on
screen will show which move it is. At this scale (the eye 68 up, a unit ~2.3 cm) a jump lands
up to a clean drop (120) below. The numbers below are from before the rescale.

Numbers to start from (units: the cycle is 50 tall, the eye 35 up, a grid cell 250):

- **A clean drop:** up to ~400 (a little over two storeys at this scale). You land, the eye dips
  and you carry on.
- **A long drop staggers you:** further than ~400. You land hard:
  - the camera jolts and sways, and the view tears briefly (the breach's glitch);
  - you're slowed to a creep for ~1.5 s, unable to run;
  - it's loud: unauthorised direct access. The drone is drawn towards where you landed, not
    just made suspicious, so it's heading for you while you can't run.
  
  It's a choice, not a punishment: a long drop is the quickest way down, and it costs you being
  found. A lip with a long drop below has no red marking.
- **An endless drop** (off the city's edge, or into a freed block's hollow) is still a reset.
- **Vault reach:** up to 250 across (a grid cell) and from 40 above to 400 below where you left.
  In the high bank, ~375 across.
- **The arc is decided when you leave.** The whole flight is flown ahead, on a copy of you, as
  you reach the lip; nothing steers it once you've left (you can turn to look). It's predictable:
  if it took you once, it always will.
- **Landing:** a dip of the eye (as `breach_drop`), a glassy thud, and you keep your speed. The
  thud is loud: the drone hears it. So vaulting is quick but noisy, and dropping is quiet but slow.

## Your body

On the cycle you were only energy. After the crash you're a person, and you see it: your hands
thrown out as the cycle goes down, your arms on the glass where you lie, a hand lifted in front of
your face, and, looking back, all of you in the mirror.

- **One body, two views** (`game/body.h`, built 2026-11-10). One skeleton is posed once a frame and
  drawn from wherever it's looked at: your own eyes and the mirror. No separate "arms in the HUD":
  what you see of yourself is what the mirror shows.
- **Poses are four points.** Each thing you do says where the two hands and two feet go; they ease
  there, and the elbows and knees follow. A new move is four points, not an animation.
- **Designed for your eyes.** Honest arms are mostly out of sight, so the run pumps the hands up
  into the bottom of the view; in the air they come up at the corners; sliding, your legs go ahead.
- **Skins.** How it looks is apart from how it moves (`body_skin`: colours per part now; widths and
  textures later).
- **Hands on the edge.** A vault puts your left hand on the top as you go over; a climb, both hands
  on the ledge's edge while you pull up past them. Where they go comes from where the walker is
  (its feet are on the top as the move starts), not from the block: resize or move the city's
  blocks and the hands still land.
- **Shape.** Each bone is one strip that tapers (forearm to wrist, torso to waist), joined to the
  next at a mitred edge, so limbs bend without notches; the hand has a palm and fingers.
- **Next:** a climb slow enough to watch your hands do it; textures per part, if the skin wants them.

## The watchdog drone

A real watchdog timer resets the machine unless it's fed regularly. That's both the enemy and
the rule.

- **It rides the towers' faces up and down,** between memory slots (floors), like a lift. Each
  drone has a route: a list of (tower face, floor, how long to wait).
- **Its scan is a sheet of light across one floor's height,** sweeping like a CRT's scanline.
  You're seen if you're within its sheet's height, inside its cone, and no tower is in the way.
  So you hide by being on another floor, or behind something on the same one.
- **Arc Raiders-style tells,** heard as much as seen:
  - Patrol: a steady hum. Its pitch is its height and it's panned to where it is (the SCSP), so
    you can follow it without seeing it.
  - Suspicious (it heard a run or a landing): it stops, turns its sensor to the noise, and the
    hum rises.
  - Alarm (it sees you): it locks on. A second or so of grace, then the reset.
- **What it hears:** a run is noise it turns towards. A long drop is unauthorised direct access:
  it comes for the place you landed.
- **Being caught is a reset.** A glitch, then you're back at the last checkpoint: the last place
  the watchdog was fed, a lift (below) or a lit pad on a roof you pass over. It fits the theme and
  needs no death animation.
- **Later:** a spotter drone, like Arc Raiders' Snitch, that only spots you and calls the
  watchdog.
- **Its model:** ~40-60 flat-coloured polygons, the corridor's colours. The beam as a mesh-pattern
  quad or a VDP2 colour effect, not VDP1 half-transparency (slow).

### The watchdog in Low Work RAM (the user, 2026-11-17)

The ask, in seven points: warn the player without words; patrol in small loops or back and forth
between junctions, three before the CD block (the third round its lift); a beam that's always
seen (a square cone); a light red glow while seen; chase, hide, search, or escape by lift; crouch
and more hiding places after; randomness in its stops, turns and flight.

Decided with the user:

- **The warning is a near miss on you,** not a scene played on someone else: the first
  watchdog's beam sweeps up to the entry roof, stops just short of you, red at its edge, and turns
  away (built third).
- **It chases over the roofs,** straight at where it last saw you. Walls hide you; they don't stop
  it.
- **Caught is a reset to the last checkpoint:** the entry roof, or the last lift used.

How it's built, step by step:

1. **Three patrols, the beam, the glow.**
   - Patrols (`city.c`, `city_patrols`): 1, a loop round the block south of the entry (columns
     0-2, rows A-C); 2, back and forth along row C between the crossroads at column 4 and the
     corner at column 8; 3, a loop round the CD block's lift.
   - It flies, not slides: it steers its yaw towards the next stop (a turn of at most
     DRONE_TURN_MAX a frame, the error's tangent from a sine and a cosine, one division a drone a
     frame) and moves along where it faces, so it banks round corners.
   - Randomness, from a seeded generator per drone (films stay the same run to run): each stop's
     wait is 0.5 to 1.5 times the route's; at a stop it may glance a quarter turn aside; it may
     turn back the other way round its route; each leg's speed differs; it weaves; now and then it
     hesitates in flight.
   - The beam is a square pyramid, its apex at the sensor, its base a square on the roofs ahead.
     You're seen inside it, with nothing in the way. Each side's drawn only if it faces you (the
     same plane test), so from anywhere outside at least one side shows.
   - The glow: VDP2's colour offset, a little red added, easing in fast and out slow.
2. **The chase, the search, the catch, the lift.** Built 2026-11-17 (`game/drone.h`, its moods):
   - Seen, from any mood, it chases: straight at where it saw you, 5 a frame (your run's 6),
     turning twice as fast as on patrol, 350 over your eye, and never under 150 over the towers
     within 400 of it. While it sees you it holds back 840 (you in its square's middle).
   - Unseen for 20 frames, it goes right over where it saw you last and searches for 8 s: turns
     to look a random way, hops up to 500 off, looks again. Then back to its patrol, clear of the
     towers on the way.
   - The catch counts up a frame in its beam and down a frame out of it: at 120 (2 s) you're
     caught. Terminated, as a fall: the terminal screen, then your checkpoint.
   - The checkpoint is where you got up, until the lift takes you; then the lift's pad (the lift
     waits for you to step off). While the lift has you, no watchdog sees you: a chase loses you
     there.
3. **The near miss.**

## On the Saturn

- **Towers are boxes:** a few flat quads each, drawn with upright lines (SKILLS.md). A dozen in
  view is well under 100 polygons. The near ones need the corridor's tricks: tops cut just above
  the screen, ends cut to a cone in front.
- **Vastness from VDP2:** the grid far below on the rotating plane (RBG0), and fog. SGL drops
  polygons past ~8192 units: distant towers and the chips on the horizon (STORY.md's hub) go on a
  VDP2 layer.
- **The walker gets a height.** Its area becomes a list of roofs, each a rectangle and a height
  (a few bytes each: 64 roofs is under 1 KB). Edges, drops and vaults are worked out against
  them. Pure C, unit-tested like `walker.h` now.
- **The drone's sight is a test against the same boxes:** a line from its sensor to your eye,
  checked against the towers' rectangles and heights. Once per drone per frame, not per pixel.

## Build order

Each step is playable, tested and filmed before the next.

1. **Roofs and heights.** The walker stands on roofs of different heights. It stops at lips,
   drops when you push, falls, and lands with a dip, or with a stagger after a long drop. A grey
   box: a few towers at the corridor's end, and a lift.
2. **Running and vaulting.** The run from holding forward, the vault's reach test and arc.
   Unit tests for both (a gap just in reach vaults; one just out of reach skids).
3. **Towers drawn properly.** Boxes, the cuts for near walls, fog, the grid below on VDP2.
4. **The drone's route and sight.** Riding between floors, the scan's sheet, the sight test.
   Done 2026-11-14 (game/drone.h; drawn with its look already, at the user's ask: drone_draw.c).
   Its scan became a beam angled down from high over the street, a CRT's line sweeping across the
   roofs and lighting them, rather than a level sheet through one floor's height (the user's ask):
   you hide in a tower's shadow, out of its fan, or under its nose.
   Pure C first; drawn as a box.
5. **Noise, suspicion, alarm, reset.** Long drops call the drone. Lifts as safe zones and
   checkpoints. This is where it becomes a game.
6. **The gates.** Each area's way in, blocked until reached or unlocked. Built as the areas are.
7. **Sound.** The hum's pitch and pan, landings, the vault.
8. **The art pass.** The drone's model, runner vision, hex addresses, the two districts.

## Still to decide

- **Which skill opens which area,** and the order the first visit allows. Probably only one area
  is open at first, so the first choice is a small one.
- **Is a lift ever risky?** For example, the doors take a moment to open with the drone close.
- **Where the arena sits:** in sight from the first rooftop (STORY.md: "a small fenced box in
  the middle of the city")?
