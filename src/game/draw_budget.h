#ifndef __DRAW_BUDGET_H__
#define __DRAW_BUDGET_H__

/*
 * The draw budget (docs/BACKLOG.md, "Stabilise graphics", SG-8): when VDP1 can't finish a frame,
 * the frame after leaves out what it can do without, so the roof under your feet isn't the part
 * that goes missing.
 *
 * Why the roof: VDP1 has no depth buffer, so jo sends the polygons far first and near last. A
 * frame VDP1 can't finish is shown as far as it got, and what's missing is the nearest: the roof
 * you stand on, cut off in a diagonal (the user, 2026-11-15, at 0660; tests/e2e/roof_cut_run.steps).
 *
 * What can go, in the order it goes: the farthest first, and never what you play with.
 *   1. the landmarks on the horizon and the A-bus (level_sky_draw's extras): ~23k of ~550k
 *      pixels in the worst frame running across 0650. Not the skyline ring drawn with them: it's
 *      the background itself, and leaving it out showed the sky's pink horizon instead of the city
 *      (the user, 2026-11-15, the first version of this);
 * That's all, for now. The arena's walls were second (~69k pixels at 0650), but on foot in the
 * city "the arena" is the pyramid, and with it the corridor's walls: from the CD block's lift,
 * where VDP1 is shortest, the pyramid was gone most of the time (the user, 2026-11-15). Like the
 * skyline, it's a landmark you steer by, not detail.
 * Never: the towers and roofs, the skyline, the pyramid, your body, the watchdog's drone, the HUD.
 *
 * How it decides, once a frame, from one fact: did VDP1 finish the frame before?
 *   - No: leave out one more part, at once.
 *   - Yes, for BUDGET_HOLD_FRAMES in a row: bring one back.
 *   - If a part brought back makes VDP1 miss within BUDGET_HOLD_FRAMES, the bringing back
 *     failed: it goes again, and the wait before the next try doubles (up to BUDGET_HOLD_MAX),
 *     so a view just over the line misses a frame now and then, not every second. A try that
 *     holds halves the wait again. (Mimas Engine's lesson: step back up slowly, or the picture
 *     flickers between two states.)
 * VDP1's own drawing time would say how much room there is, but in a normal build there's no
 * reading of it (jo's profiler has one; game/vdp1_gauge.h says why its "busy" isn't room): so
 * this learns from misses instead.
 *
 * Pure C, no jo: unit-tested on the PC (tests/test_draw_budget.c).
 */

// The parts that can be left out, in the order they go
typedef enum
{
	BUDGET_LANDMARKS = 0, // the landmarks on the horizon and the A-bus (never the skyline)
	BUDGET_PARTS = 1	  // (a new part goes before this, the nearest last: tests/test_draw_budget.c)
} budget_part;

#define BUDGET_HOLD_FRAMES 60 // finished frames before a part comes back (1 s at 60 fps)
#define BUDGET_HOLD_MAX 1024  // the longest wait after failed tries (~17 s)

typedef struct
{
	int dropped;	  // how many parts are left out: the first `dropped` of budget_part's order
	int clean;		  // finished frames in a row
	int hold;		  // finished frames needed before the next part comes back
	int since_return; // frames since a part came back (-1: none on trial)
} draw_budget;

void draw_budget_init(draw_budget *b);

// Once a frame, before drawing: whether VDP1 left the frame before unfinished (vdp1_gauge's
// last_unfinished)
void draw_budget_frame(draw_budget *b, int vdp1_unfinished);

// Whether this frame draws the part
int draw_budget_allows(const draw_budget *b, budget_part part);

#endif
