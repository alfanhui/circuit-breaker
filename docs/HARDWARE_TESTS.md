# Real Saturn test plan

Everything so far has run in emulators (Mednafen in Docker, YabaSanshiro on the PC). Emulators
copy the hardware closely but not perfectly, and they hide some costs completely. This is the
list to work through on the real console, most important first.

Written 2026-10-28, for a session about a month later. Fill in the **Result** lines as you go;
photos and short phone videos are the most useful thing to bring back.

## Before you go

- [ ] **Commit the work first**, so the disc matches a known commit. Write the commit hash
      (`git log -1 --format=%h`) on the disc or in the results below.
- [ ] Build: `cmd /c "scripts\compile.bat"`, then check `game.iso` is new.
- [ ] Wall and trail fog has its own switch, `WALL_TRAIL_FOG` in `main.c`: if test 1e
      shows problems, a third disc with it `false` separates the two fogs.
- [ ] **Burn a second, fallback disc without the fog**, in case it breaks on the Saturn (test 1):
      set `#define FLOOR_FOG false` in `main.c`, build, burn, label it "NO FOG", then set it back
      to `true`. Commit with it `true`.
- [ ] Also commit the jo-engine BSS fix (`jo_engine/core.c`, outer repo) or at least make sure
      the build uses it. Without it, only a quarter of the game's variables start at 0. Emulators
      usually start with RAM full of zeros and hide this; a real Saturn doesn't (see test 7).
- [ ] The build is **NTSC, for a Japanese Saturn**. The fog assumes 263 rows per frame (NTSC);
      a PAL console would run with 313 and the fog would be wrong.
- [ ] Bring: a phone that films at 60 fps or more (120/240 fps slow motion is better), and a
      way to photograph the screen straight on.

Commit on the disc: `________`

## The debug text, for reference

Lines counted from the top (the layout as of 2026-11-04):

| Line | Shows |
|---|---|
| 1 | `fps`, `cpu` (one-second average), `pk` (worst frame), `late` (frames that missed 60 fps) |
| 2 | `poly` given / displayed, `vert`, and the bots' and trails' worst frame (`bots`, `trl`) |
| 3 | view (`1ST`, `ISO` or `CAM`), position, heading, boost |
| 4 | cruise `spd`, `arena` size, debug switches |
| 5 | the first bot, the round |
| 6 | the level's parts, worst frame each (`ar co sd tp fl gr bd wk`: utils/perf.h) |
| 7 | VDP1's own gauge: `over` (frames it didn't finish), `busy` (% of frames it was still drawing when the CPU was done), `cmd` (its list's length), `v` (its chip version). See `game/vdp1_gauge.h` |

Pad: Up boost (the cycle always drives) · Left/Right turn · Down 1st person / iso · A new trail · X worst-case trail ·
Y arena boundary · Z turn camera compare · B/C speed · L/R arena size · Start debug camera.

Read `cpu` at least ~2 seconds after changing anything: it averages over a second.

---

## 1. Floor fog to black (highest risk)

**What it is.** The floor fades to black with distance. VDP2 blends the floor with black by a
ratio, and the console's DMA rewrites that ratio at the end of every screen row (HBlank), with
a restart every frame from VDP2's row counter. Code: `src/floor_fade_vdp2.c`.

**Why it's risky.** Three of its details came from reading Mednafen's source, not from a
Saturn: how the row counter is read, how it counts during vertical sync, and that the DMA
reads memory 4 bytes at a time. If any of them differs on the real console, it shows here.

### 1a. Does it look right?

- [ ] 1st person, facing a far wall: the floor darkens smoothly to black where it meets the
      wall's base. Rows near the bottom of the screen are clear, with no banding.
- [ ] 1st person radar (bottom right): a tilted hologram disc, fully on screen (CRTs crop the
      edges)? Readable at a glance? It should turn smoothly with the view on every turn.
- [ ] The radar's glass is a mesh (every other pixel). Over composite or RF it should blur into
      see-through; over S-video or RGB it will look like a checkerboard. Which does it look like?
- [ ] Iso (Down): the top of the floor fades to black; the area round the cycle is clear.
- [ ] Iso: crash into your trail and slide along it with the trail on the camera's side. The cube
      should stay behind the trail (only its top showing) all along, no flicker at any spot. The
      cube on its own should show 3 faces, none missing.
- [ ] Switch views several times: the fade changes to match each time.
- [ ] The sky over the wall tops (1st person) is its normal purple-to-pink: no darkening or
      stripes from the fog, which shares its VDP2 screen.
- [ ] Compare against the emulator shots in `tests/build/e2e` (`dark_north`, `dark_iso`) if
      they're still there, or run `tests\e2e\run_e2e.ps1 -Steps dark.steps` for fresh ones.

Result: ________

### 1b. What a failure would look like, and the likely cause

| You see | Likely cause | What to bring back |
|---|---|---|
| Fog in the right shape but shifted up or down a fixed amount | The row counter counts differently during vertical sync, so the restart lands a few rows early or late (`hblanks_before_picture`) | Photo; roughly how many rows off (the debug text rows are 8 pixels tall: use them as a ruler) |
| Fog jumps up and down from frame to frame | The row counter isn't latching when EXTEN is read (`current_row`), so the restart reads a stale value | Video |
| Horizontal stripes, or the fade repeats down the screen | The DMA's read address drifts: hardware reads memory differently from Mednafen (the 4-byte entry rule) | Photo |
| No fog at all, floor at full brightness everywhere | DMA not starting on HBlank, or SGL resets the colour calculation | Photo |
| Whole floor black, or one flat shade | DMA writing the wrong value, or once per frame instead of per row | Photo |
| Fine normally, but the top of the screen flickers or goes wrong when the game slows down (X trail, test 3) | The once-per-frame restart ran late, after the picture had started, and was skipped (`floor_fade_vblank` returns early), so the DMA ran past the table | Video while holding X trail |
| Hang, or corrupted graphics, sound or loading | SGL or jo also uses the fog's SCU DMA level and the two clash. It was level 2, which SGL streams PCM sound on (in Mednafen every sound stopped the fog); it's level 0 now, which SGL uses only for CD reads | When it happens, and whether the NO FOG disc does it too |
| Fog fine until the first sound (the loop's break, a crash), then gone or flickering | Something else still shares level 0 | Which sound, and whether it comes back |
| Single fog rows a line out (a step in the fade) that come and go, worst when the scene is busy | SCU precaution 12: CPU writes to the VDPs (SGL's command list, the wall fog's tables) come before SCU DMA, so that row's transfer started late. Or precaution 20: three DMA levels running at once (ours, SGL's level 1, sound on level 2) lose their priority order | Video; does it follow busy scenes, or sounds? |
| A hang that the NO FOG disc doesn't have | SCU precaution 23: a DMA level's registers rewritten while it runs hangs the SCU. `arm_dma` turns the level off first but doesn't wait for a transfer in flight | When it happens (often or rare) |

### 1c. If it's broken

Use the fallback disc (see "Before you go"), so the rest of the game can still be tested. It's
built with `#define FLOOR_FOG false` in `main.c`: the fog is never set up and its DMA never
starts, so the floor is at full brightness everywhere (checked in Mednafen: both views look
like the floor did before the fog, and the game runs normally).

If the fallback disc *also* misbehaves in the same way, the fog isn't the cause.

### 1d. Its cost

- [ ] `cpu` in 1st person and in iso should be about the same as with fog off (Mednafen: 5% and
      2%, no difference measurable). The DMA steals a little memory bandwidth per row; check
      the figures don't climb.

Result: 1st ____ % · iso ____ %

---

## 1e. Wall and trail fog (Gouraud)

**What it is.** Walls and trails dim with distance, matching the floor: VDP1 Gouraud shading,
a small colour table per quad in VDP1's RAM (`src/object_fog_vdp1.c`). Since 2026-11-04 a wall or
trail piece uses one of 512 tables written once at start-up (its corners are two levels: `game/
depth_fog.h`); only faces cut by the view on foot get one written per frame (0-1 a frame in
Mednafen, was one per fogged quad). Each write stops VDP1 drawing (SEGA's VDP1 manual).
Switch: `WALL_TRAIL_FOG` in `main.c`. Emulator check only so far.

- [ ] Look back along a long trail (lay one, then U-turn): it fades from bright cyan to dim teal.
      Compare with `tests/build/e2e/object_fog_compare.png` (`object_fog.steps`).
- [ ] Far walls dimmed, near walls bright, no step between neighbouring pieces.
- [ ] **Flicker on walls or trails while moving or turning.** Each frame's tables go in the
      other half of a two-set buffer, assuming VDP1 draws one frame behind the CPU. If that's
      wrong on the real console, pieces would flash the wrong brightness for a frame.
- [ ] **All four walls shaded the same** (e.g. a near wall as dark as a far one): the walls
      share attribute tables and rely on SGL reading them during each draw call.
- [ ] **Garbage on textures** (stray coloured blocks): the tables would overlap texture memory.
      The game checks at start-up; it would show an error instead.
- [ ] CPU with the X trail, fog on vs off (`object_fog_perf.steps` in Mednafen: 1st person
      36% off, 42% on; iso 20% off, 27% on).
- [ ] VDP1's `busy` (debug line 7) riding beside a wall and along a long trail, fog on vs off.
      Mednafen showed no difference from the shared tables (it may not stop VDP1 for CPU
      writes); on the Saturn the old per-quad writes would have. If fog on still costs VDP1 much
      more than off, it's Gouraud itself (the backlog's "Gouraud fog's cost: re-check the cause").
- [ ] Does it look good? Does dimming the far walls lose the arena's sense of size?

Result: ________

---

## 2. Turn camera feel (ease-out)

**What it is.** In 1st person the cycle turns 90 degrees instantly (so trail corners stay
square); only the camera eases round, covering 6/16 of the remaining angle each frame. Code:
`src/game/turn_ease.c`. Z steps through EASE, EASE SLOW (1/8 speed), LINEAR SLOW, LINEAR.

- [ ] EASE vs LINEAR at full speed: can you feel the difference? Which feels better?
- [ ] On a CRT, the fast first frames of the ease-out may smear more or less than on a PC
      monitor. Is the turn readable, or does it feel like a cut?
- [ ] Turn rapidly left-right-left: does the camera keep up, or feel behind?
- [ ] Slow motion (EASE SLOW): presses made between the slowed frames should still all count.
- [ ] Film a turn at high frame rate if you can: from button press to the first camera movement
      should be 1 frame (about 17 ms). More means input lag.

Result: ________

---

## 3. Speed: CPU and dropped frames

The CLAUDE.md rule: the PC hides the cost of division and floats, so figures must come from
the Saturn. The emulator figures below are the ones to compare against.

| Scene | How | Mednafen | Saturn |
|---|---|---|---|
| 1st person, open arena | Start, drive | ~5% | ____ |
| Iso | Down | ~2% | ____ |
| Worst-case trail, 1st person | X, then drive alongside it | 31% avg, 60 fps, 0 late | ____ |
| Worst-case trail, iso | X, Down | | ____ |
| Shrinking the arena | Press L several times | 6% avg / 17% peak (unexplained) | ____ |
| Close to a wall, facing along it | Drive to a wall, turn parallel | | ____ |

- [ ] `fps` stays at 60 in each; note any `late` counts.
- [ ] Do you see stutter that `late` *doesn't* count? On the PC that was blamed on the emulator's
      pacing; on the Saturn it would be a real problem worth chasing.

### 3b. VDP1's own gauge (debug line 7)

It reads VDP1's status registers (SEGA's VDP1 manual, chapter 4), so on the Saturn it measures
the real chip; Mednafen's VDP1 timing is only an estimate.

| Scene | Mednafen (2026-11-04) | Saturn |
|---|---|---|
| On foot, the corridor's start | `over 0 busy 0% cmd 238`, cpu 76% | ____ |
| On foot, the city's roof | `over 0 busy 100% cmd ~95`, 60 fps, cpu 48% | ____ |
| On foot, looking back at the doorway from the city | `over 0 busy 94% cmd 282`, 37 fps, cpu 82% | ____ |

- [ ] Does `over` ever go above 0? That's a frame shown half-drawn: note the scene.
- [ ] If `busy` is much higher on the Saturn than in Mednafen, Mednafen is kinder to VDP1 than
      the chip is, and every Mednafen frame rate is optimistic.
- [ ] VDP1's version, the line's last figure (`v1`): Mednafen reports 1. A version 0 chip
      lacks the speed bits the backlog's Pclp and HSS rows want.

---

## 4. Walls, trails and the 3D (VDP1)

- [ ] **Up close to walls**: drive right up to a wall, along it, and into the corners. Walls
      must not stretch, flip or spray across the screen. (VDP1 coordinates wrap outside
      -1024..1023; `wall_clip` keeps them in range. The emulators may wrap differently.)
- [ ] **Long trails**: build a long trail and drive alongside it, close and far. Pieces should
      stay straight, not bend diagonally. (Backlog: "retest".)
- [ ] **Far walls**: from the arena's centre, look at each far wall. Do the grid lines stay
      steady while driving, or crawl and jump? (Backlog: far-wall texture LOD.)
- [ ] **Iso and tall walls**: in iso, drive near the south and east walls. Does a wall ever hide
      the cycle?
- [ ] **Flicker or missing polygons**: anything dropping in and out, especially far walls
      (the scene goes close to SGL's ~8192 depth limit).
- [ ] **Glitches that move around randomly** (garbage polygons for a frame): could be the second
      CPU reading stale data (backlog: slave SH-2 cache coherency). Note when it happens.
- [ ] **The grey box's stairs** (on foot, `BOOT_ON_FOOT 1`): walk up to the first step and stop,
      looking straight up the stairs; then the platform's end, facing the tower. In Mednafen
      both read 60 fps (the stairs' foot was 30 before the box fronts were laid flat:
      `lay_wide_pieces_flat`, level.c). Note `fps` and `cpu` at each. 30 at the stairs' foot on
      the Saturn would mean VDP1's line cost isn't what Mednafen models.
- [ ] **The tower's front up close**: run to the tower on the low roof, stop, turn a little each
      way. Mednafen: 60 fps (30 before its bottom was cut to the screen: `cap_band_bottoms`).
      Look for the floor showing through the tower's or a wall's foot, mostly looking down from
      the platform: the cut's worked out for a 240-row picture; a gap means the Saturn shows more.
- [ ] **Down the corridor, from where you stand up**: `cpu` read 78-81% in Mednafen. Laying far,
      small fronts flat made it slower there (60 to 54 fps) for no reason worked out: if the
      Saturn's quicker with them flat, `LAY_FLAT_LINES` (level.c) can come down.
- [ ] **On foot, everything's cut to the view in 3D** (`game/frustum.h`, 2026-11-03): turn slowly
      on the spot at the start, by the tower, and out on the city's entry roof, looking up and
      down too. Look for a crack or a one-pixel gap along the screen's edges (a cut there is a
      sixteenth past the edge), a box side or tile popping in or out at the edge, and spikes (a
      corner VDP1 wrapped). The cut leaves things up to twice the screen's width uncut: if the
      Saturn's slower than Mednafen at the stairs' foot or facing the tower, that guard band
      (`FRUSTUM_GUARD_X16`) is the first thing to narrow.
- [ ] **The seams are VDP1 lines** (`glass_outline_add`): a thin white line round each tile, near
      you, fading out with the fog. Look for lines that flicker, break up, or show through things
      standing on the floor (they're chained right after their tile). Mednafen: 60 fps at the
      start and the stairs' foot.
- [ ] **The ground under the city is black** on foot (the VDP2 floor's palette): no shimmer of
      navy and white while turning out on the roofs. Looking back from the city through the
      doorway you see the corridor's floor and stairs; Mednafen shows 38-44 fps there.

Result: ________

---

## 5. Floor look on a TV

- [ ] Over composite / S-video / RGB (note which): is the near floor blocky? Does the far grid
      shimmer, or does the TV soften it enough? (Backlog: "judge on a CRT".)
- [ ] Is the black of the fog and the space beyond the walls truly black, or does the TV lift it
      to grey? Bold contrast is part of the art direction.
- [ ] Iso: outside the south and west walls the floor grid carries on (known). How bad is it?

Result: ________

---

## 6. Controls

- [ ] **Double tap for a U-turn**: does the short wait for the 50-unit gap feel like the turn
      you asked for, or like lag? Running back beside your trail, it should look solid.
- [ ] **Crash into a trail**: drive a box and steer back into your own trail. The cycle should
      stop dead with the trail filling the view (no hole, no flicker), stay stopped while facing
      it, and pull away from a standing start after turning. Same against the arena edge.
      Crash, jink left a little, right into the trail again, then right once more: the cycle
      must stay put, not drive back over the bit it just laid.
      Then turn to run along the trail: it should reach the screen's edge, nothing showing past
      it. The trail is now cut 10 in front of the camera (and the camera moved 20 forward, onto the cycle): watch for its near corners jumping
      or tearing (VDP1 coordinate overflow), right beside a trail and stopped facing one.
- [ ] **Rapid taps**: tap Left five times fast. Every tap should turn (presses are latched so a
      slow frame can't lose one).
- [ ] **Boost** (hold Up): does +75% feel like a kick at every cruise speed (B/C)? Is 1.5 s
      long enough, and the half-second pause before the refill noticeable but fair?
- [ ] **The view widening while boosting**: felt as speed, or as a lurch? Any wall pieces
      popping in at the screen edges as it widens (culling is told the wider view)?
- [ ] **The gauge, round the radar's rim** (bottom centre): readable from the corner of the
      eye? It drains from the top down both sides. Colours distinct: white boosting, dim amber
      in the pause, amber refilling or full. In iso the rim shows on its own.
- [ ] **Turn and boost together**: both now come from the same pad sample, so they should
      land in the same frame. Does turn + boost on the same press feel right?
- [ ] **Unplug the pad mid-game** and plug it back: the game should carry on, not hang or turn.
- [ ] If you have a 3D Control Pad (the analogue one), try it in digital mode (switch at "+").
- [ ] **Tuning**: with B/C (speed) and L/R (arena size), find what feels best on the TV.
      Write the numbers down; they'll be baked in.

Result: speed ____ · arena ____ · ________

---

## 7. Start-up and memory

- [ ] **Cold boot, several times**: power off for ~10 s, then on. The game should start the same
      way every time. Real RAM holds random values at power-on, and emulators usually start with
      zeros, so a variable that's never set shows up only here: odd start positions, a trail
      already drawn, debug flags already on, or a crash.
- [ ] Boot from the disc menu and from a reset (the reset button): same result?
- [ ] **Loading time** from the Saturn logo to the arena: ____ s. (The CD is far slower than an
      emulator's.)
- [ ] All textures present: walls (`WALL.TGA`), floor. A missing file shows as a blank
      or white surface.

Result: ________

---

## What to bring back

- The results above, with the commit hash.
- Photos or videos for anything wrong in test 1, labelled with what you did.
- The `cpu` / `late` figures from test 3.
- Your pick of EASE or LINEAR, and your speed and arena numbers.

The more exact the description ("fog 2 text rows too high, steady"), the quicker the fix.
