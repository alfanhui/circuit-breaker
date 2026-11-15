# Building a level that VDP1 can draw

How to build the next level (and fix this one) so it holds 60 fps without trial and error. It's
what the city taught us, 2026-11-15, turned into steps. The history and the bigger plan are in
[CULLING.md](CULLING.md); the backlog rows are SG-5 to SG-8 and SG-12 in [BACKLOG.md](BACKLOG.md).

## The one fact to design around

**VDP1 pays for pixels, not polygons, and it draws everything it's given.** It has no depth
buffer: jo sends the polygons far first, near last, and each one is filled whether a nearer one
will cover it or not. jo's own figure for its speed is about 28 pixels a microsecond: a field
(16.7 ms) is about 450k pixels at the very most, before jo's own commands and the time the list
takes to arrive. In the city, VDP1 ran out at about 550k (jo's estimate, which counts a quad as
its lines x its widest line). The screen is 320 x 240 = 77k. So:

- **A frame may cost about 5 screens' worth, no more.** Aim for 350k in the worst view.
- **When VDP1 runs out, the nearest is what's missing**: the floor under your feet, cut off in a
  diagonal (the user's gif at 0660). It looks like a floor bug; it isn't.
- **Hidden pixels cost the same as seen ones.** Most of the work is in leaving out what's hidden.

## The tools (all in the game already)

| Tool | What it tells you | How |
|---|---|---|
| `px` debug line | VDP1's pixels by part, worst frame a second (thousands) | `JO_COMPILE_WITH_PROFILER_MODULE = 1` in the makefile (comment on its own line: SKILLS.md). Row 9. utils/perf.h `perf_px_mark` to add a part |
| Wireframe | What's drawn behind what: the layers | `jo_3d_set_wireframe(true)` for one build, a film's shots |
| `vdp1 over N/T` | Frames VDP1 didn't finish: this second / since boot | Debug row 6. T at a film's end compares two runs of the same moves |
| `bg` | Parts the draw budget has left out | Debug row 7 |
| Benchmark | Fixed views, pass or fail on their own | `run_e2e.ps1 -Steps bench.steps` |
| A running film | The worst frames: moving, vaulting, mid-jump | `roof_cut_run.steps`: copy it for a new place |
| Glitch checks | Flashes, stray colours, holes in every frame | `record`, `flashes_from/to`, `holes_from/to` (SG-6) |

**Measure moving, not only standing.** The benchmark's views stand in a roof's middle and read
`over 0` where running across the same roofs misses frames. The worst frames are mid-jump: over a
street nothing hides the street's depths, and you really can see them.

## The order to leave things out

Each step is cheaper per object than the next, so do them in this order. Each one errs towards
drawing: when it can't be sure, it draws.

1. **Whole places, worked out ahead.** For each place you can stand, a table of what can be seen
   from it at all (`city_seen`, `tools/make_city_seen.py`; the corridor's doorway table). One bit
   test an object a frame. *A new level: write its table first.*
2. **What's off the screen.** Whole boxes against the view (`frustum_classify_box`), then faces.
3. **What a nearer, taller thing hides.** The skyline's hiders (`game/skyline.h`): a tower behind
   a nearer, taller one isn't drawn, a side partly behind one is trimmed.
4. **What the floor you stand on hides** (`game/floor_hides.h`, new). From your eye, the floor
   hides a cone below it. An upright face is cut at the cone's height at each end; a box whose top
   corners are all in the cone isn't drawn. Four divisions a frame, then multiplies. Works for any
   floor that's a rectangle you're over. In the city: the slave's share of the pixels fell from 138k
   to 38k at 0650, and the towers' sides stop at your roof's edge instead of the street. Wherever a
   level has a floor you stand on with things below it, call `floor_hides_begin` for it and use
   `floor_hides_below` / `floor_hides_box` where its faces are made.
   *Combine limits as whole lines, never end by end* (`floor_hides_line`): a face's foot is the
   higher of its own foot and the hidden height at every point, and that has a corner where they
   cross. Taking the higher at each end and joining them cuts across the corner: a wedge cut out of a
   wall standing on a lower roof (the user, 0650, `gap_run.steps`). A line is used only if it's above
   the face's own foot at both ends. The same holds for the screen-edge limits on the top and foot.
5. **Cut to the screen.** Faces reaching far past the screen are cut at its edge (our cuts and
   jo's guard band). A smaller guard band made no difference here: our cuts already do it.
6. **The draw budget, last** (`game/draw_budget.h`). What's left is what shows. When VDP1 still
   can't finish, the next frame leaves out the optional parts, farthest first (the landmarks, then
   the arena), and brings them back slowly. It reacts to a miss, so the first miss still shows: it's
   a safety net, not the plan. *A new level: list its optional parts in that enum, farthest first.
   Never optional: the floor you're on, what you can touch, the enemy, the HUD, the background
   that stands in front of VDP2's sky (the skyline): leaving it out shows the sky's horizon colour
   instead, which reads as a glitch. Nor landmarks you steer by (the pyramid): "far off" isn't
   "optional" if it's how you know where you are, and the draw budget acts most where VDP1 is
   shortest, so it took the pyramid away most of the time at the CD block's lift. Check what a draw call contains before making it optional:
   `level_sky_draw` built the skyline and the landmarks as one mesh.*

## Designing the geometry

- **Floors you stand on: rectangles, lined up with the axes.** Then step 4 applies as it is.
- **Tall faces in rows are the expensive picture.** Two rows of tall walls down a narrow street
  (0650 looking along it) fill the screen with walls, and every wall behind them in the row is
  filled too unless a hider trims it. Break long canyons with a turn, or make the walls behind
  lower than the ones in front, so step 3 can trim them.
- **Big near faces are fine when they hide**: a wall filling the screen is one screen's worth. It's
  the layers behind it that cost.
- **Jumps show the depths.** Mid-air over a gap nothing hides what's below: keep what's down there
  cheap (flat colours, few faces, VDP2's floor for the very bottom), and keep gaps narrow.
- **Optional detail far off**: landmarks, far skyline strips, things on the horizon. Make them
  separate draws so the budget can leave them out.

## Checking a new place

1. A running film through it, recorded (`record`), with the glitch checks.
2. The `px` line in a profiler build: the worst frame's total under ~350k, and by part where it went.
3. `over N/T` at the film's end: 0 is the aim; a few with `bg` above 0 is the budget catching it.
4. A benchmark view for it (stood), so a later change can't make it worse unnoticed.
5. The wireframe, once: anything drawn that's always hidden is a step 1-4 job.
