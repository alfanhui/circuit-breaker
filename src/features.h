#ifndef __FEATURES_H__
#define __FEATURES_H__

/*
 * The game's switches, in one place: where it boots, what's drawn, what's heard, what debug shows.
 * Each is a constant, set here and built. The ones marked "(pause menu)" are only where the game
 * starts: START pauses, and the menu changes them while playing (L the next one, R changes it;
 * game/settings.h). The others set the VDPs up once at boot, or are the boot itself: a build is
 * the only way to change them.
 *
 * Commit with the boot settings as they are below (the main menu: SCENE_INTRO, 0, 0, 0). The E2E
 * scripts read and set some of these lines (tests/e2e/run.sh, run_e2e.ps1, film_round.ps1): keep
 * each "#define NAME value" on its own line, as it is.
 */

// --- Where it boots --------------------------------------------------------------------------

/*
 * The scene the game boots into (input.c, scene): SCENE_INTRO, the prologue (the opening flight,
 * then the spectator's view of the loop: three presses break it and start play; the game's "main
 * menu"); SCENE_LOOP; or SCENE_PLAY, straight into play, to work on it. While it runs, START (the
 * debug camera) then A jumps to the next part. tests/e2e/run.sh reads this line (skip_prologue,
 * start_intro)
 */
#define BOOT_SCENE SCENE_INTRO

// The round play starts in, from 0 (game/rounds.h): to work on a later round, set it and build, so
// its bots and layout are there from the start (tests/e2e/film_round.ps1 sets it for a film)
#define BOOT_ROUND 0

// 1: the game boots stood up in the corridor behind round 5's flicker, as if you'd just got up
// after the designed crash (input.c, boot_on_foot): for working on Work RAM without riding there
// first. Films don't depend on it: tests/e2e/run_e2e.ps1 builds with whichever a film needs
// (wait_boot: the arena, 0; wait_up: the corridor, 1) and puts it back afterwards
#define BOOT_ON_FOOT 0

// With BOOT_ON_FOOT: 1 stands you on the CD block's hub instead, the far end of the way down
// through the city (game/city.h), for working there without the 28-tower walk
#define BOOT_AT_HUB 0

// With BOOT_ON_FOOT: a city tower's address (docs/images/work_ram_plan.svg: 0x0650 is row 6,
// column 5) to stand in the middle of, facing back towards the corridor, for working on a spot
// there; -1: none. A film asks for one with a "boot_at 0650" line (tests/e2e/run_e2e.ps1 builds
// with it and puts it back)
#define BOOT_AT_TOWER -1

// --- What's drawn ----------------------------------------------------------------------------

// The purple-to-pink sky above the walls. It's the second half of the floor's VDP2 screen (RBG0
// plane B), so the floor fog's per-row blend reaches it too, but every row above the horizon
// gets fog level 0 (glow_rows 0), and the 1500 high walls always reach above the horizon, so
// the sky shows only over the wall tops, untouched. false: black beyond the walls instead
#define SHOW_SKY true

// The floor fades to black with distance (src/floor_fade_vdp2.c). It's only been checked in
// emulators: false builds a fallback disc for the real Saturn with the floor at full brightness
// everywhere and the fog's DMA never started (docs/HARDWARE_TESTS.md, test 1)
#define FLOOR_FOG true

// Walls and trails dim with distance too, matching the floor (src/object_fog_vdp1.c), with VDP1
// Gouraud shading. A prototype to judge: Gouraud can only take 16 of 31 steps of brightness off,
// so far walls dim rather than vanish. false: walls and trails at full brightness everywhere
#define WALL_TRAIL_FOG true

/*
 * Which shape a cycle is (main.c), at boot (pause menu). Four to choose from while we decide
 * (docs/BACKLOG.md)
 */
#define CYCLE_RIBBON 0 // a ribbon twisting from level into the trail's upright edge
#define CYCLE_TOKEN 1  // a diamond spinning on its point
#define CYCLE_CURSOR 2 // a terminal's block cursor, blinking
#define CYCLE_LINKED 3 // the token, joined to its trail by a still ribbon
#define CYCLE_SHAPE CYCLE_LINKED

// The city's towers' sides reach down to the board, not 900 under the eye (level.c, grey_side_add;
// pause menu). Filmed both ways at the hub (hub.steps): no difference the films' own swing didn't swamp
#define CITY_SIDES_TO_BOARD 1

// --- What's heard ----------------------------------------------------------------------------

// The sound effects (src/sound.c; pause menu): the loop's hits, the turns' whoosh, the cycle's hum.
// false: none is played (silent, for working on something else); they're loaded all the same, so
// the menu can turn them on. Whatever this says, they're silent while the game's paused
#define SFX_ON true

// --- Debug -----------------------------------------------------------------------------------

/*
 * 1: a benchmark build (game/bench.h; docs/BACKLOG.md, SG-5). It boots on foot and, with no pad,
 * goes round a fixed list of views, counting the frames each misses, then turns the whole screen
 * green (every view held its frame rate) or red, with a line a view. Run it with
 * tests/e2e/run_e2e.ps1 -Steps bench.steps, which builds with this on, reads the colour and puts
 * it back. 0 to play (and to commit)
 */
#define BENCH 0

/*
 * 1: a glitch-check build (docs/BACKLOG.md, SG-6). VDP2's back colour, what shows where neither
 * VDP1 nor a VDP2 plane draws, is magenta instead of black: a colour nothing in the game uses, so
 * a hole in the level stands out to tests/e2e/glitch.py's holes check. run_e2e.ps1 builds with it
 * on for a film with a holes_from step, and puts it back. 0 to play (and to commit)
 */
#define GLITCH_CHECK 0

/*
 * The draw budget (game/draw_budget.h, SG-8): when VDP1 can't finish a frame, the next leaves out
 * the landmarks, then the arena's walls while you're on foot in the city. 0 draws everything
 * always: for comparing films with and without it (tests/e2e/roof_cut_run.steps)
 */
#define DRAW_BUDGET 1

/*
 * What the roof you stand on hides (game/floor_hides.h): the towers' sides behind it aren't drawn.
 * 0 draws them all: for comparing films with and without it
 */
#define FLOOR_HIDES 1

/*
 * SG-9: a wall that covers the whole screen hides everything behind it (game/screen_wall.h; level.c,
 * level_screen_wall_limit). 0: no depth limit, for comparing films with and without it
 */
#define SCREEN_WALL 1

/*
 * The city's blocks shared between the two CPUs (level.c, grey_groups_draw): the slave builds and draws
 * every other one. 0: the master does all of them, as before: for comparing films
 */
#define GREY_ON_SLAVE 1

// The debug readout, top left: fps, cpu, polygons, the cycle, the bots, the parts' costs (main.c,
// debug_3d; pause menu). false: none, as a player would see it
#define SHOW_DEBUG_TEXT true

/*
 * The opening flight's debug lines (main.c, debug_flight), at the screen's top:
 *     ARRIVE 2/4 overview>swoop 128 x2
 *     cycle 6400 500 cam 7100 1800 400
 * the segment playing (of 4), the keys it's between (game/intro_flight.c), the frame (of 512)
 * and how hurried it is (input.c); then where the cycle is, and the camera (x, y, height), to
 * place the keys by. false: no lines, as a player would see it (pause menu)
 */
#define SHOW_FLIGHT_DEBUG true

#endif
