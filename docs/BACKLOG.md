# Backlog

Proposed fixes and features, so tangents don't lose track of anything.
Newest requests at the top of each section. Move an item to **Done** (with its commit) when it lands.
Bigger items also belong on the GitHub Project board; this file keeps the detail and the "why".

Effort: S = under an hour, M = a session, L = several sessions.

## Now (being worked on)

| Item | Status | Effort |
|---|---|---|
| Long trails bend diagonally | Fixed in code: trails drawn as short pieces near the player (`trail_pieces`). Bending in normal driving not captured on screen: **retest** | M |
| Stutter that the FPS counter doesn't show | `late` counter added (debug line 11). Long trails did drop to 32 fps; now 60. If you see stutter and `late` doesn't move, it's the emulator's pacing on the PC: **retest** | M |
| Docker E2E still waits ~15 s for the BIOS | Mednafen has no emulated BIOS and fast-forward can't go faster in the container | S |

## Next: movement feel (from the turn and boost review)

| Item | Why | Effort |
|---|---|---|
| Ease-out 1st person turn camera | Linear 10-frame turn feels mechanical; ease-out answers at once and settles with weight | S |
| Boost scales with cruise speed | Boost is a fixed +7: at cruise 20 it's only +35% | S |
| Boost feedback: FOV kick in 1st person, HUD gauge | Gauge empty / full is invisible outside the debug text | M |
| Retune boost: longer, refill pause after use | 0.67 s of boost then 3.3 s of waiting may feel like a blip | S |
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
| Per-dot floor fog (coefficient line colours) | The per-line fade only suits a level camera (1st person). Writing a line colour into each rotation coefficient (K_LINECOL) would fade by true distance in any view, and could darken as well as brighten | L |
| Iso haze tints the floor but not walls/trails | VDP1 isn't in the line colour mix; the far floor goes pink, a far wall in the same rows doesn't. Line colour insertion for sprites (LNCLON) could fix it | M |
| Iso cutaway shows the floor grid outside the arena | VDP2's floor is endless; past the south/west glow line it carries on | S |
| Unused BOX1-4.TGA on the disc | No longer loaded (walls use WALL.TGA); delete when sure | S |
| jo: every 8-bit VDP1 sprite uses the first TGA palette | Why WALL.TGA is 24-bit. Worth understanding before more 8-bit sprites | S |
| Debug camera pitch vs the fade | The fade table is built for the 1st person pitch; pitching the debug camera (B/C) leaves it in the wrong rows | S |
| Raise the 1st person eye height (35) | Lower eye = more magnified floor near the bottom of the screen | S |
| Judge on a CRT / the real Saturn | Emulators show sharp square pixels; a TV over composite / S-video softens them a lot | - |

## Next: scale

| Item | Why | Effort |
|---|---|---|
| Curved world (proposed 2026-10-27) | Floor and polygons bend away like a small planet: the horizon becomes a real edge, the far shimmering floor drops out of view, and less is drawn. Floor: rewrite VDP2's per-line rotation coefficients (needs research). Walls/trails: drop each column by distance^2 * k (multiply + shift). Hard part: the two curves must match exactly. Trade-off: a strong curve hides the far walls | L |
| Fog on walls and trails | The floor fades in 1st person; walls and trails (VDP1) don't, and walls still end at 6000 | M |
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
| Iso camera centred on the cycle (offset 456, worked out from height and pitch) | uncommitted |
| View culling: walls and trails clipped to what each camera can see (`view_cull`), not a fixed square. Iso sends 70 -> 19 polygons along a wall, 274 -> 142 with a full zig-zag | uncommitted |
| Walls visible across the whole arena: own view range (28000), level of detail (250 / 1000 / 2000 pieces by distance), world handed to SGL at 1/4 scale (SGL stops drawing a little under 8192 deep), slZdspLevel 7 (was an invalid 0) | uncommitted |
| Trails clipped to stay in front of the 1st person camera like the walls (`trail_clip_columns`, reusing `wall_clip_strip`); straight segments skip divisions | uncommitted |
| Walls bent up close: each piece is a low (0-250) and high (250-1500) strip, clipped to stay in front of the 1st person camera and inside VDP1's -1024..1023 pixel range (`wall_clip`); iso cutaway draws the camera-side walls as just the glowing base | uncommitted |
| Walls grounded: generated grid texture (`tools/make_wall.py`) continues the floor grid up the wall, glowing base and top edge; replaces the BOX debug textures | uncommitted |
| One floor grid (125) in every view; iso moire softened by fading the far upper floor (fade now handles a tilted camera) | uncommitted |
| Floor distance fade, 1st person: per-line additive haze via VDP2's line colour screen (`floor_fade`) | uncommitted |
| Floor: generated 256x256 texture (`tools/make_floor.py`) with soft-edged lines, ~2 texels per unit, 125-unit tiles; unused VDP1 copy of FLOOR.TGA dropped | uncommitted |
| Walls 6x taller (1536) | uncommitted |
| YabaSanshiro fast boot script (`scripts/run_fast_boot.bat`, emulated BIOS) | uncommitted |
| Mednafen script: our BIOS, Japanese region | uncommitted |
| This backlog | uncommitted |
| 8x arena, walls drawn near the player, pad tuning (B/C speed, L/R size) | uncommitted |
| Latch button presses so a slow frame can't lose a turn | c585c6c |
| E2E: LF line endings, wait for a visible window | f7495b2 |
| NTSC build for the Japanese Saturn | 7d0ffc5 |
| Centred start, boundary toggle, Z limits (#6) | e030331 |
