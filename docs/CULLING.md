# Culling and drawing: the long-term plan

Written 2026-11-03, after a week of the same problem in new places: each new piece of level
(the grey box, the stairs, the tower, the city's first 21 towers) cost frames, and each time the
cause was found by switching things off one at a time and filming. That works, but it doesn't
scale to a city of ~130 towers and a drone. This is the plan for making cost predictable.

## What keeps going wrong

| Symptom | Cause | Where it was patched |
|---|---|---|
| 30 fps at the stairs' foot | VDP1 line set-up: wide quads drawn with upright lines | `lay_wide_pieces_flat` |
| 30 fps facing the tower | VDP1 filling pixels off the screen | `cap_band_bottoms`, `grey_side_crossing` |
| Wall drawn over the stairs | Painter's sort by polygon centre | walls `SORT_MAX` |
| Towers drawn through the corridor's walls | `SORT_MAX` walls behind everything | the doorway wedge (`grey_portal`) |
| 30 fps (CPU 99-128%) down the corridor | Every box looked at every frame; 64-bit maths | culls in `grey_box_drawn` |

Three things underlie all of them:

1. **Every frame starts from nothing.** Each box is tested against the camera, its sides cut,
   its corners converted, whether or not it could ever be seen from where you stand. The cost
   grows with the size of the level, not with what's on screen.
2. **Order is decided per polygon by SGL's depth sort,** which is wrong for big polygons and
   for rooms with insides and outsides. Each fix (`SORT_MAX`, the wedge) is local to one place.
3. **We can't see where the time goes,** so each fix starts with guessing. The debug line has
   timed parts (`perf.h`: `bots`, `trl`), but nothing for the level.

## The plan, in order

Each step stands on its own and is worth doing even if the later ones never happen.

### 1. Measure every part (small, first)

Add `perf.h` parts for the level: corridor walls, boxes' sides, boxes' tops, the glass floor,
the walker. Show them on the debug line (a second line of parts). Then every film says where
the CPU went, and VDP1's share is what's left (cpu low and fps low: VDP1).

- Also a **perf check in the E2E films**: a debug flag that turns the debug line's fps red when
  under 58, so a contact sheet shows a regression at a glance. Later, a steps file per known
  hard view (`stairs_perf`, `tower_perf`, `city`) run after any drawing change.

### 2. Cells and portals, worked out ahead (the main change)

The level is static, so what can be seen from where can be decided **before** the frame, once,
not every frame. This is how Quake and most PS1/Saturn-era 3D games did it (a potentially
visible set, PVS).

- **Cells:** the level split into regions you can stand in: the corridor in 3 or 4 lengths,
  each city tower's roof, each street. On the plan that's natural: the grid *is* the cells.
- **Portals:** the openings between cells (the doorway, a roof's open edges).
- **Per cell, a list:** which boxes can be seen from anywhere in it, and through which portal.
  Worked out by `tools/work_ram_plan.py` (Python, on the PC, no cost to the Saturn) and written
  out as C tables with the map. A few bytes a box, per cell.
- **At run time:** find your cell (the walker already knows the roof it's on), walk its list.
  Boxes not on it are never touched. The cost is what's visible, however big the city gets.
- **The doorway wedge (`grey_portal`) becomes the general case:** a cell's list says which boxes
  need cutting to which portal, so the cutting only runs where it's needed.
- **Order comes with it:** a cell's list can be written far to near, by portal, so the drawing
  order is decided offline per cell rather than by sorting each polygon's centre. Big boxes
  drawn in list order with `SORT_BFR` chains, or in a few depth bands, instead of `SORT_CEN`.

### 3. A coarse cull before the fine one

Before any box is looked at closely: a test per block (2 x 2 towers, one bounding square) against
the view's wedge, then per tower, then per side. Cheap integer tests (the `view_cull` module has
them for the arena). Within a cell's list this cuts it further to what's in front of you.

### 4. Work out once what never changes

Per box, at load: which of its four sides can ever be seen (one against a neighbour as high is
never drawn, today it's still looped over), its corners as SGL points, its colours. Per frame
only what depends on the camera (the cuts to the screen). Already started: `grey_boxes_work_out`.

### 5. The walker's roofs in a grid (the FPS demo's two-level index)

`walker_area` is one list, checked in full by every query (the jump's flight runs ~64 frames of
them). It went from 30 to 51 rectangles with the slice; the whole city is ~250. Index it by
grid cell (the plan's grid again): a query checks only the roofs in its cell and the ones beside
it. `Projects/FPS Collision demo` does the same with its two-level collision index.

### 6. Far away goes to VDP2, not VDP1

VDP1 is the limit as often as the CPU. Beyond ~3000 units, towers are a few pixels each but still
cost a polygon set-up and sorting. Draw the distant city as a skyline on a VDP2 layer (as the
sky is): a few pre-made strips, chosen and scrolled by heading. Nearer towers stay polygons.
WORK_RAM.md already plans the far chips on VDP2.

### 7. Rules for drawing a big quad (already learnt, kept in one place)

Today they're spread over `level.c`'s comments and SKILLS.md:

- fill lines: lying for wide square-on pieces, upright for tall or tapering ones;
- cut tops and bottoms to just past the screen, and only where both ends of an edge are cut;
- no corner far off the screen (VDP1 wraps, and walks off-screen pixels);
- no 64-bit maths and no division per polygon in the frame.

Move the box drawing (sides, tops, cuts, fill lines) out of `level.c` into its own module, with
these rules in its header, so a new kind of level object gets them for free.

## Recommended order

1. **Measure** (step 1): a session. Without it every later step is guessed again.
2. **Box drawing into its own module** (step 7): it's needed by everything after.
3. **Cells and portals** (step 2), with the coarse cull (3) and work-once (4) as part of it.
4. **The walker's grid** (step 5): before the slice grows past ~60 roofs.
5. **The far city on VDP2** (step 6): once more of the plan is built and the far towers count.

## Right now

**Step 1 is built** (2026-11-03): the debug line's 6th row times each part of the level, its
worst frame in the last second as % of a frame (`utils/perf.h`, `main.c`):
`ar` arena walls, `co` corridor walls, `sd` box sides, `tp` box tops, `fl` glass floor, `gr` the
VDP2 grid, `bd` SGL taking the wall strips, `wk` the walker. Film: `tests/e2e/parts.steps`.

The first readings, in Mednafen (the city's entry blocks in, uncommitted):

| View | fps | cpu | ar | co | sd | tp | fl | gr | bd | wk |
|---|---|---|---|---|---|---|---|---|---|---|
| Where you stand up, down the corridor | 30 | 100 | 4 | 13 | **27** | 7 | **33** | 0 | 1 | 0 |
| The same, turned to a side wall | 60 | 44 | 7 | 6 | 2 | 3 | 13 | 0 | 0 | 0 |
| On the stairs | 60 | 83 | 3 | 7 | **29** | 16 | 14 | 0 | 0 | 0 |

**The glass floor, fixed in part (2026-11-03):**

- each tile takes one fog level from its centre and is drawn in that darker colour
  (`depth_fog_darken`), with no Gouraud table written into VDP1 RAM and no per-pixel shading;
- `corridor_clip`'s division at each edge crossing is 32-bit (`edge_share`: the sums brought
  down to 14 bits first, as magnitudes so two tiles sharing an edge still meet);
- each corner's side of a quad edge is worked out once, an edge with every corner inside is
  passed over, and a polygon wholly outside one is dropped;
- no 64-bit shift by a varying count (a library call).

Where you stand up: 30 fps, cpu 100, floor 33 became **55 fps, cpu 85, floor 21**. The near
tiles' cut was 11 of the floor's 24 before the last step. Still to do for the floor: the corners
worked out once (step 4) and only the rows ahead looked at (step 2). The box sides (27) are next.

**The box sides, fixed in part (2026-11-03):** timed inside, adding each side's strip (its cut
to the view cone, its corners) was 15, the caps and the fill-line check 7, the crossing column 3.

- a box wholly well in front and in view (`grey_box_whole`: the tops' test, and its height and
  feet within 4 x its distance of the eye's, inside VDP1's range) has its sides added uncut: none
  of the cut, caps, crossing or checks. Down the corridor that's all 20 stair fronts;
- the cone cut tests whether an end is already in front and inside the cone on the scaled sums
  before dividing (`wall_clip.c`): most are, and each strip was ~20 divisions for nothing;
- box sides are fogged by their colour, one level a piece (`flat_fog`), not a Gouraud table each.

Where you stand up: sides 27 became **13**, and it's **60 fps, cpu 72**.

Found filming the run: at the crate's jump the walker (`wk`) reads 22% for its frame (47 fps
that second): the jump's flight flown ahead, up to 64 frames of it, each checking all 51 roofs.
That's step 5 (the walker's roofs in a grid). The corner of the tower looking up is still 30 fps
with cpu ~40%: VDP1.

So down the corridor, the glass floor (~100 hexagons placed and cut every frame) and the boxes'
sides (25 boxes plus the towers, each side cut, capped and checked) are three fifths of the frame.
The walker, the grid and SGL's own work are nothing. Those two are where steps 2-4 start:
the floor's tiles and the boxes' sides listed per cell and worked out once, not every frame.

## One clipper, VDP2 for the background only (2026-11-03, uncommitted)

The user found four artefacts on foot: a black band over the hexagons at the start; box sides
cutting in from the bottom of the screen while turning; looking back from the city, the corridor
gone (white); turning in the city, the ground flickering between navy and white. Each came from a
local trick:

| Artefact | Cause |
|---|---|
| Black over the hexagons | the seams were the VDP2 floor showing through, and on foot it lay 3000 down under the city: far rows fogged black |
| Sides cut from the bottom | the bottom caps and crossing column (floor-plan cuts that assume a level eye) |
| Corridor gone from the city | its floor and boxes weren't drawn from outside at all |
| Ground flickering | the VDP2 grid 3000 down: its lines far too fine at that distance, the plane sampled lines or cells as you turned |

A Saturn developer whose 3D game looks stable described theirs: all level geometry is VDP1
quads; VDP2 is only the back screen and glows; rooms are joined by portals, and a closed door
closes its portal so the room behind isn't drawn. What we took from it:

- **One clipper for everything** (`game/frustum.h`, unit-tested): each face is cut in 3D by planes
  from the eye: near, the screen's four edges, and a portal's edges. Exact whichever way you look,
  so none of the floor-plan special cases are needed on foot (cones, caps, crossing columns, bands
  looking down). The boxes, the city's towers, the corridor's walls and the glass floor all go
  through `level_face_add` (level.c).
- **Portals as planes**: the doorway's two sides and its sill are three planes from the eye
  (`grey_portal_set`). From inside, the city is cut to them; from the city, the corridor's boxes
  and floor are. Nothing below the sill is drawn from inside (it's under the floor).
- **VDP2 is the background on foot**: the floor's palette goes black (`main.c`,
  `set_floor_colours`). The seams are VDP1 lines on the tiles (`glass_outline_add`).

What it cost, and how it was won back (Mednafen, `parts.steps`, cpu %):

| Step | Start | Stairs' foot |
|---|---|---|
| Before (old cuts) | 60 fps, 72 | 60 fps, 83 |
| Every face cut at the screen's edges, each box by its 8 corners | 30 fps, 106 | 30 fps, 101 |
| `1 << i` and `>> 14` out of the clipper (library calls on the SH-2) | 30, 94 | 30, 101 |
| Cut only at a guard band (2 x the screen); boxes by centre and size (`frustum_classify_box`) | 30, 85 | 30, 85 |
| A white floor under the tiles for the seams: VDP1 filled the floor twice | (30, VDP1) | (20, VDP1) |
| Seams as polylines instead, one line a seam | 60, 82 | 30, 84 |
| Boxes tested in groups first (the stairs, the city's rows) | 60, 76 | 30, 80 |
| Faces reaching past the guard band cut at the screen's edge (stair fronts' lying lines) | **60, 76** | **60, 79** |

The stairs, tower and city films hold 60 fps (55 in two city frames). Looking back from the city
at the doorway is 38-44 fps: the corridor's insides are drawn there now (they weren't), and the
arena's walls behind it are still drawn by the old path, unseen behind the corridor's.

Still to do from here: the tiles' seams as textures on the tiles (as the other game's floors
are), the arena's walls culled from the city, and cells with lists worked out ahead (step 2),
which the doorway's planes are the first piece of.

## Occlusion: what a wall hides (2026-11-04, uncommitted)

VDP1 has no depth buffer, so a face hidden behind a nearer one is still sent and filled. Looking
down the corridor from the city, all the arena's walls were drawn behind the corridor's: 65
polygons, ~22% of the CPU, for nothing. `game/occluder.h` (unit-tested) is one shape for two jobs:
an upright rectangle and the eye make a pyramid (4 planes, `frustum_add_edge`) plus the
rectangle's own plane.

- **A wall** hides a box wholly inside all five (`occluder_hides`): the corridor's three walls.
- **A hole** shows only what's not wholly outside the four sides (`occluder_shows`): from outside
  the arena, below its walls' tops, its inside is seen only through the hole you broke, above
  the breach glass. This is the doorway portal's trick, used the other way round.

What made it pay (it first cost more CPU than it saved):

- Test the biggest thing first: a whole side's span, a whole block, a whole run. One test hides
  them all, before their pieces are worked out.
- A hidden end of a run is found by **halving**: if the first m pieces are hidden as one box, so
  are fewer. About 5 tests, not 30. A piece at a time was ~400 tests a frame.
- A wall's own plane is tested first, and each test stops at the first plane that fails: most
  boxes are on the near side of most walls.
- Check the result against a build with the thing switched off: the "identical without the
  arena" judgement was wrong at first (its top rises above the corridor's walls and is seen).

Next: the corridor's boxes as occluders (the tower and the stairs hide the far end), and the
city's towers once there are many (they'd fit cells and portals, step 2, better).

## The doorway from the city (2026-11-05, uncommitted)

Looking in through the doorway from the city's roof was the last view short of 60 fps (54-55,
cpu ~87%), and the roof beside it was 30 (VDP1). Each fix leaves out work the doorway already
makes pointless; none cuts anything new:

- **The floor's tiles a column at a time.** They're laid in columns along the corridor, so a
  column is tested as one box first: wholly outside the doorway, its tiles are skipped in one
  test; wholly in view, they aren't tested at all. ~3% of the CPU.
- **Only the doorway's planes that can hide the floor.** The sill is the floor's own far edge, so
  the whole floor (and all that stands on it) is inside it while the eye's above the floor. The
  side at the corridor's side wall hides nothing unless you stand beyond that wall. Tested
  anyway, the last column of tiles crossed the sill and was sent to the clipper for nothing
  (`test_frustum.c`, `a_doorway_s_sill_and_far_jamb_hide_no_floor`).
- **The side walls behind the far end's wall.** From the city, a side wall you're not beyond is
  seen only through the doorway, so a piece wholly outside the doorway's planes is left out
  (only left out, never cut: the far end's wall is drawn over what's behind it, as before). Little
  here (from the doorway most of both walls really is seen), more from further off to one side.
- **Tiles only as far as they're outlined.** Past `GLASS_OUTLINED` (1000) the tiles are a few
  pixels each in the fog through the doorway; from the city the far glass takes their place. This
  was the big one: half the tiles, ~5% of the CPU.

What didn't help, so isn't in: dropping the outlines (35 polygons, no CPU: SGL's cost is per
point, and the outlines share the tiles'), and cutting the boxes' tops exactly at the screen's
edge for the VDP1-bound roof (60 with or without, once the rest was in).

The part timings (`ar co sd tp fl ...`) were a poor guide here: each is a second's worst frame,
timed by the FRC, so an interrupt that lands in a part (SGL's vblank work) counts in it. One-off
builds and the `cpu` and `late` figures settled it.

## Looking down the corridor from the yard (2026-11-06, uncommitted)

The yard (corridor.h: the run's moves to try) is on the corridor's right half, and from there you
see the whole corridor: 45-58 fps, cpu 80-87%. Without the yard's boxes it was no better (55-58):
the view, not the yard. One-thing-off builds, each worth 10-15% of the CPU there: the stairs, the
corridor's walls, the floor's tiles. What went in:

- **The stairs far off as a ramp** (`grey_stairs_ramp`, past 1000): twenty steps, ~40 faces, each
  about a pixel high from there. Two faces now: the slope through the steps' front edges, and the
  open side's profile. Only the drawing: you still walk the steps.
- **Their open side as one profile, nearer** (past 400, seen from beside): each step's side was a
  whole rectangle from the floor up, twenty overlapping faces for one wall. The profile cuts the
  steps' corners (8 x 12), under 3 pixels from there.
- **Tiles only as far as they're outlined, everywhere** (1000 either way, was 1750 ahead): as the
  doorway's fix, the far glass in their place.
- **The corridor's wall pieces 500 near you where there's no fog to follow** (both ends fog level
  0): half the near pieces, the ones always cut at the screen's edges.

Now 59-60 down the yard (cpu 65-67%), but for the second you climb the ledge (VDP1 at 96%: its
face, then its top, fill the screen with everything behind still drawn).

What didn't, and isn't in:

- **The nearest tall box as an occluder** (`game/occluder.h`, the corridor's boxes hiding what's
  behind them). VDP1's busy fell (96 to 0-73%), but the tests cost more CPU than they saved (cpu up
  ~7%, 47 fps by the stairs), and limited to near faces and whole columns it made no difference.
- **Box tops cut exactly at the screen's edge** (for the ledge's top at your feet): worse, a second
  time. The run up the left half fell to 48 at the crate.
- Mednafen's cpu figure moves ±4% between builds that change nothing in the view (code moving in
  the cache): compare within one film, and trust a change only if it moves more than that.
