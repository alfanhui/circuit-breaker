# Backlog

Proposed fixes and features, so tangents don't lose track of anything.
Newest requests at the top of each section. Move an item to **Done** (with its commit) when it lands.
Bigger items also belong on the GitHub Project board; this file keeps the detail and the "why".

Effort: S = under an hour, M = a session, L = several sessions.

Items to check on the real console are gathered in [HARDWARE_TESTS.md](HARDWARE_TESTS.md).

## Now (being worked on)

| Item | Status | Effort |
|---|---|---|
| Long trails bend diagonally | Fixed in code: trails drawn as short pieces near the player (`trail_pieces`). Bending in normal driving not captured on screen: **retest** | M |
| Stutter that the FPS counter doesn't show | `late` counter added (debug line 11). Long trails did drop to 32 fps; now 60. If you see stutter and `late` doesn't move, it's the emulator's pacing on the PC: **retest** | M |
| Docker E2E still waits ~15 s for the BIOS | Mednafen has no emulated BIOS and fast-forward can't go faster in the container | S |

## Next: movement feel (from the turn and boost review)

| Item | Why | Effort |
|---|---|---|
| Tune the boost by feel | Numbers in cycle.h (+75%, full in 8 frames, 1.5 s, 0.5 s pause, 3 s refill) and view.c (FOV +12 degrees, eased 2/16 a frame): judge in play and on the Saturn | S |
| Boost juice beyond the view | Sound (Cesta-like glassy whoosh), the trail brightening while boosting, a flash when the gauge is full again | M |
| Floor fog during the FOV kick | The fog rows are worked out for 90 degrees: while boosting the fog sits up to ~15% nearer. Rebuild the table per FOV step if it shows | S |
| Cycle heading visible in iso view | The cube is symmetrical: only the trail shows direction. Nose polygon or camera lead | S |
| Crash on walls and trails (collision) | Hitting a wall just stops the cycle; in Tron it's a crash. Board: "Add collision detection logic" | L |
| Decide: double-tap U-turn, or grid-snapped turns | Two quick turns lay a 7-unit stub; with collision that's suicide | S (decision) |
| One input source | Turns read raw SMPC registers, boost uses jo's pad read; they can be a frame apart | S |

## Next: floor pixelation (proposed 2026-10-27)

The floor is VDP2's rotating background (RBG0): a 64x64, 8-bit FLOOR.TGA tiled every 64 world units,
1 texel per unit, no filtering. Near the low 1st person camera each texel is a big block; far away
1-texel lines break into shimmering dots.

| Item | Why | Effort |
|---|---|---|
| Per-dot floor fog (coefficient line colours) | The per-row fog suits a camera that doesn't roll. Per-dot ratios from the rotation coefficients would follow true distance in any view | L |
| Decide: keep wall and trail fog? (`WALL_TRAIL_FOG`) | Prototype in (see Done). Judge the look, and whether far walls dimmed by half still give the arena its scale | S (decision) |
| Wall and trail fog: cheaper | Costs ~6-8% CPU with the worst-case trail in Mednafen (~200 cycles a quad). Ideas: inline the level maths (two calls per corner now), build the tables in work RAM and DMA them to VDP1 in one go (CPU writes to VDP1 RAM may stall while it draws), skip the far wall LOD pieces' tables (all full fog: share one table). Measure on the Saturn first | M |
| Wall and trail fog: darker than half | Gouraud can only take 16 of 31 steps off. Far pieces could also switch to Gouraud + half-luminance (VDP1 colour mode 6), but the switch shows as a step between pieces | M |
| Iso cutaway shows the floor grid outside the arena | VDP2's floor is endless; past the south/west glow line it carries on | S |
| Unused BOX1-4.TGA on the disc | No longer loaded (walls use WALL.TGA); delete when sure | S |
| jo: every 8-bit VDP1 sprite uses the first TGA palette | Why WALL.TGA is 24-bit. Worth understanding before more 8-bit sprites | S |
| Debug camera pitch vs the fade | The fade table is built for the 1st person pitch; pitching the debug camera (B/C) leaves it in the wrong rows | S |
| Raise the 1st person eye height (35) | Lower eye = more magnified floor near the bottom of the screen | S |
| Judge on a CRT / the real Saturn | Emulators show sharp square pixels; a TV over composite / S-video softens them a lot | - |

## Next: scale

| Item | Why | Effort |
|---|---|---|
| Curved world (proposed 2026-10-27; prototype tried and reverted) | Floor and polygons bend away like a small planet: the horizon becomes a real edge, the far shimmering floor drops out of view, and less is drawn. Prototype (reverted, not committed): `world_curve_drop` (r^2 * strength >> 24, no division), Z steps strength 0 / 16 / 32 / 64, walls and trails only. Findings: drops are right on the Saturn and cost nothing measurable, but (1) far walls sink onto the flat floor in front of its horizon, so the floor must bend too (VDP2 per-line coefficients, needs research); (2) the far wall grid breaks into offset blocks, see far-wall texture LOD. Trade-off: a strong curve hides the far walls | L |
| Far-wall texture LOD | Far wall pieces squeeze the 125-unit grid into ~3 px, so VDP1 drops texture rows. Identical pieces drop the same rows (flat world: looks fine head-on), but any height difference between pieces (curve, or a wall seen at an angle) drops different rows and the lines jump. Fix: a coarser wall texture (fewer, thicker rows) for the 1000 / 2000 LOD pieces | M |
| Longer trails | 64 corners and 60 high is small in a 16000 arena | M |
| Investigate CPU spike when shrinking the arena | 6% avg / 17% peak in the shrink E2E shot, unexplained | S |
| Trail build cost on the debug zig-zag | 1st person worst case is 31% CPU (60 fps, 0 late): building pieces ~20%, clipping ~8%. Straight segments now skip divisions; the zig-zag's long diagonal head still divides per column. Measure a realistic many-turn trail before optimising more | M |
| `cpu` debug figure is a one-second average | Read it at least ~2 s after a change (E2E steps now wait). Earlier 'unexplained' iso 17% and 'clip costs 21%' readings were taken too soon | S |
| `displayed` debug counter is unreliable | SGL's DispPolygons is updated by the slave CPU; sampled at frame start it sometimes reads 0 | S |
| E2E: early button presses get lost | Presses sent while the game is still loading never arrive (e.g. 5 of 7). Wait for the game, not a fixed delay | M |
| Trail drawing CPU cost | Worst case (full zig-zag) is now 27% avg / 35% peak, up from 7%: more vertices. Fine today; watch with 2 players | S |
| Trail draw buffer holds 200 columns | ~12 KB. A full zig-zag shows only its newest rows near the player | S |
| Iso camera vs tall walls | Walls are 1536 high, the iso camera 768: near the south/east walls they may hide the cycle | S |
| Settle default cruise speed and arena size | Tune with B/C and L/R on hardware, then bake in | S |

## Later / platform

| Item | Why | Effort |
|---|---|---|
| jo bug: `JO_VDP2_LINE_COLOR_TABLE` is allocated at the start of VRAM B1, on top of SGL's text font (`jo_printf`) | Overwrote the glyphs (screen of stripes). Worked round by allocating in A0; report upstream | S |
| Send the BSS clear fix upstream to jo-engine | `jo_engine/core.c` clears a quarter of BSS. Fixed locally (outer repo, uncommitted) | S |
| `JO_WORK_RAM_SYSTEM_WORK` looks like a typo (0x06FFC00) | Probably harmless; low confidence | S |
| Slave SH-2 cache coherency | Only if SGL hands work to the slave: stale data, visual glitches | M |
| Close board issue #5 (trails), card the optional arc | Trails done except the arc from the cycle | S |
| E2E: wait for the game instead of fixed delays | Fixed waits break on a slow machine (seen once) | M |
| Push over SSH from this machine | No SSH agent here; pushes go from your terminal | S |

## Done

| Item | Commit |
|---|---|
| Boost reworked: +75% of the current speed at any cruise (was a fixed +7), a full gauge lasts 1.5 s (was 0.67), refill waits 0.5 s after the button is let go (tapping can't top it up), 3 s to refill. The boost builds over 8 frames (0.13 s) and fades over 16; it snapped to full in 2, and the view widening with it looked like a glitch. Feedback: the 1st person view eases up to +12 degrees wider (`ease_out_step`, 2/16 a frame, never a whole degree in one frame; culling and `wall_clip` widen with it), and a gauge bar at the bottom (`src/hud.c`): white boosting, dim teal waiting, cyan refilling. E2E `boost.steps`, slow-motion `boost_gif.steps`; `keydown`/`keyup` steps added | 76f6c3e, 999c0ff, 59038ff |
| Never-ending sunset: generated sky (`tools/make_sky.py`). The old SKY.TGA stepped to black at texel 53, a black band across the top of the 1st person screen; now the original glow up to violet, then deepening to indigo by texel 84 and held, past the screen top (~77) | c90954b |
| Sky back on (`SHOW_SKY`), above the walls. It's RBG0 plane B, the floor's own screen, so the floor fog's per-row blend reaches it, but sky rows get level 0: checked unchanged with the floor fog on and off | ed926ac |
| Fog on walls and trails (prototype, `WALL_TRAIL_FOG` in main.c): VDP1 Gouraud shading, each corner darkened by its distance ahead along the floor, over the floor fog's own distances so walls fade with the floor they stand on (`depth_fog`, unit-tested to agree with the floor at every distance). Tables written each frame into VDP1 RAM, two sets on alternate frames. Dims by at most 16/31. Cost ~6-8% CPU worst case (Mednafen) | ed926ac |
| Fog to black on the floor: VDP2 ratio blend with a black line colour, the ratio (CCRR) rewritten every row by SCU DMA level 2 started on HBlank, restarted each VBlank from the row counter. Sky off (`SHOW_SKY`). No measurable CPU cost. Mednafen only: **check on the Saturn** | ed926ac |
| Ease-out 1st person turn camera: 6/16 of the remaining angle per frame (38% on the first frame, was 10%), min step 1.4 degrees, lands in 10 frames (the old linear turn took 11) (`turn_ease`). Z: debug slow motion and linear turn, to compare | ed926ac |
| Iso camera centred on the cycle (offset 456, worked out from height and pitch) | 7c8d9c9 |
| View culling: walls and trails clipped to what each camera can see (`view_cull`), not a fixed square. Iso sends 70 -> 19 polygons along a wall, 274 -> 142 with a full zig-zag | 7c8d9c9 |
| Walls visible across the whole arena: own view range (28000), level of detail (250 / 1000 / 2000 pieces by distance), world handed to SGL at 1/4 scale (SGL stops drawing a little under 8192 deep), slZdspLevel 7 (was an invalid 0) | 7c8d9c9 |
| Trails clipped to stay in front of the 1st person camera like the walls (`trail_clip_columns`, reusing `wall_clip_strip`); straight segments skip divisions | 7c8d9c9 |
| Walls bent up close: each piece is a low (0-250) and high (250-1500) strip, clipped to stay in front of the 1st person camera and inside VDP1's -1024..1023 pixel range (`wall_clip`); iso cutaway draws the camera-side walls as just the glowing base | 7c8d9c9 |
| Walls grounded: generated grid texture (`tools/make_wall.py`) continues the floor grid up the wall, glowing base and top edge; replaces the BOX debug textures | 7c8d9c9 |
| One floor grid (125) in every view; iso moire softened by fading the far upper floor (fade now handles a tilted camera) | 7c8d9c9 |
| Floor distance fade, 1st person: per-line additive haze via VDP2's line colour screen (`floor_fade`) | 7c8d9c9 |
| Floor: generated 256x256 texture (`tools/make_floor.py`) with soft-edged lines, ~2 texels per unit, 125-unit tiles; unused VDP1 copy of FLOOR.TGA dropped | 7c8d9c9 |
| Walls 6x taller (1536) | 7c8d9c9 |
| YabaSanshiro fast boot script (`scripts/run_fast_boot.bat`, emulated BIOS) | 7c8d9c9 |
| Mednafen script: our BIOS, Japanese region | 7c8d9c9 |
| This backlog | 7c8d9c9 |
| 8x arena, walls drawn near the player, pad tuning (B/C speed, L/R size) | 7c8d9c9 |
| Latch button presses so a slow frame can't lose a turn | c585c6c |
| E2E: LF line endings, wait for a visible window | f7495b2 |
| NTSC build for the Japanese Saturn | 7d0ffc5 |
| Centred start, boundary toggle, Z limits (#6) | e030331 |
