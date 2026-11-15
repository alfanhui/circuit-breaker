#ifndef __BENCH_H__
#define __BENCH_H__

/*
 * The benchmark (docs/BACKLOG.md, "Stabilise graphics", SG-5): a build that judges the frame rate
 * on its own, so a slow place fails a run instead of waiting for someone to notice it in play.
 *
 * With BENCH 1 (src/features.h) the game boots on foot and, with no pad, goes round a fixed list
 * of views (bench_views, bench.c): it stands you at each, holds its buttons (a look up, say), lets
 * the picture settle, then counts for BENCH_MEASURE_FRAMES frames:
 *   - late: fields that showed VDP1's picture before again (jo's late count, perf.h);
 *   - over: frames VDP1 didn't finish (game/vdp1_gauge.h);
 *   - peak: the worst frame's master CPU, as a % of a vblank.
 * A view passes with no late frame and no unfinished one. At the end the whole screen turns green
 * (every view passed, or failed only where it's known to: bench_view.known) or red, with a line a
 * view (main.c); tests/e2e/run_e2e.ps1 -Steps bench.steps reads the colour.
 *
 * This file is the counting only, pure (no jo): the game places you and draws (input.c, main.c).
 * Frames are what's counted, not time: at 30 fps a view takes twice as long, which is fine.
 */

// Frames to let a view settle before counting: the eye stands up, a look eases round, the meshes
// for the new place are built (the first frame there is slow on its own)
#define BENCH_SETTLE_FRAMES 90
// Frames counted at each view: 192, as jo's own sample checks (3.2 s at 60 fps)
#define BENCH_MEASURE_FRAMES 192

// Where a view stands you: a city tower by its address (row << 8 | column << 4, as the debug
// line's "at"), or the corridor where you get up after the crash
#define BENCH_AT_START 0xFFFF

typedef struct
{
	const char *name;	   // shown on the verdict's line (at most 12 characters)
	unsigned short at;	   // a tower's address, or BENCH_AT_START
	unsigned char quarter; // which way you face: quarter turns from facing back along the corridor
	unsigned short held;   // the pad's buttons held while there (DIGI_..., low_level_input.h)
	unsigned char known;   // 1: known to fail today (its BACKLOG row says why): it can't turn the run red
} bench_view;

typedef struct
{
	int late, over, peak;
	int vdp1_peak; // VDP1's longest drawing, in FRC counts (bench_note_vdp1), 0 if not known
	int pass; // late and over both 0
} bench_result;

#define BENCH_VIEWS_MAX 16
extern const bench_view bench_views[];
extern const int bench_view_count;

// What the game does this frame
typedef enum
{
	BENCH_PLACE,   // stand at bench_run.view now (its first frame)
	BENCH_SETTLE,  // stay there, not counted
	BENCH_MEASURE, // stay there, counted
	BENCH_DONE	   // all views done: show the verdict
} bench_action;

typedef struct
{
	int view;  // the view being run (bench_view_count once done)
	int frame; // frames at this view so far
	bench_result results[BENCH_VIEWS_MAX];
} bench_run;

void bench_init(bench_run *b);

/*
 * Once a frame, with what the frame before cost: late_vblanks (fields it repeated a picture), unfinished (1 if
 * VDP1 didn't finish it), cpu (its master CPU, % of a vblank). The figures are of the frame before
 * because that's when they're known; a view's first counted frame is BENCH_SETTLE_FRAMES after it
 * was placed, so none of the move's own cost is counted. Returns what to do this frame
 */
bench_action bench_frame(bench_run *b, int late_vblanks, int unfinished, int cpu);

/*
 * How long VDP1 took to draw its last frame, in FRC counts (jo's profiler: jo_profile_vdp1_last,
 * only built with JO_COMPILE_WITH_PROFILER_MODULE = 1). Kept as the view's peak while it's counted;
 * call it each frame after bench_frame. It doesn't judge the view: late and over do
 */
void bench_note_vdp1(bench_run *b, int ticks);

// 1 when every view passed, but for those marked known
int bench_passed(const bench_run *b);

#endif
