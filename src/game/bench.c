#include "bench.h"
#include "../utils/low_level_input.h" // DIGI_Y: the views that look up

/*
 * The views, in the order they're run. Each is a place someone found slow or wrong in play
 * (docs/BACKLOG.md), so the list is what "the game holds its frame rate" means. Quarters count
 * from facing back along the corridor (towards the arena): 0 back, 2 out towards the city, 1 and 3
 * the sides. "known" marks a view that fails today and has a backlog row of its own: it's shown,
 * but can't fail the run; take the mark off once its row is done, so the list of them only shrinks.
 * The known marks were set from the first run (2026-11-15)
 */
const bench_view bench_views[] = {
	{"start out", BENCH_AT_START, 2, 0, 0},		 // where you get up, facing the city
	{"start back", BENCH_AT_START, 0, 0, 0},	 // the same, facing the mirror
	{"mirror up", BENCH_AT_START, 0, DIGI_Y, 0}, // facing the mirror, looking up at it
	{"0650 back", 0x0650, 0, 0, 1}, // the user's slow run (2026-11-14), known: BACKLOG "Slow in the city"
	{"0640 back", 0x0640, 0, 0, 0}, // (the same row; 0 repeated fields by jo's late count, 2026-11-15: no longer known)
	{"0630 back", 0x0630, 0, 0, 0},
	{"0A40 back", 0x0A40, 0, 0, 0}, // up column 4
	{"0B40 back", 0x0B40, 0, 0, 0}, // by Backup RAM's hub
	{"hub q0", 0x0010, 0, 0, 0},	// the CD block's hub, each way round
	{"hub q1", 0x0010, 1, 0, 0},
	{"hub q2", 0x0010, 2, 0, 0},
	{"hub q3", 0x0010, 3, 0, 0},
};
const int bench_view_count = (int)(sizeof bench_views / sizeof bench_views[0]);

void bench_init(bench_run *b)
{
	b->view = 0;
	b->frame = 0;
	for (int i = 0; i < BENCH_VIEWS_MAX; ++i)
	{
		b->results[i].late = 0;
		b->results[i].over = 0;
		b->results[i].peak = 0;
		b->results[i].vdp1_peak = 0;
		b->results[i].pass = 0;
	}
}

bench_action bench_frame(bench_run *b, int late_vblanks, int unfinished, int cpu)
{
	if (b->view >= bench_view_count)
		return BENCH_DONE;
	// This frame's place in the view: the figures passed in are the frame before's, so frame
	// BENCH_SETTLE_FRAMES + 1 is the first whose figures are counted (frame BENCH_SETTLE_FRAMES
	// itself, the first measured, reports the last settling one's)
	if (b->frame > BENCH_SETTLE_FRAMES)
	{
		bench_result *r = &b->results[b->view];

		r->late += late_vblanks;
		r->over += unfinished;
		if (cpu > r->peak)
			r->peak = cpu;
	}
	if (b->frame == BENCH_SETTLE_FRAMES + BENCH_MEASURE_FRAMES)
	{
		// This view's done: judge it, and place the next at once
		bench_result *r = &b->results[b->view];

		r->pass = r->late == 0 && r->over == 0;
		b->view++;
		b->frame = 0;
		if (b->view >= bench_view_count)
			return BENCH_DONE;
	}
	return b->frame++ == 0 ? BENCH_PLACE : b->frame > BENCH_SETTLE_FRAMES ? BENCH_MEASURE : BENCH_SETTLE;
}

void bench_note_vdp1(bench_run *b, int ticks)
{
	// Counted from the same frame as the others (bench_frame has already moved the frame on)
	if (b->view < bench_view_count && b->frame > BENCH_SETTLE_FRAMES + 1 && ticks > b->results[b->view].vdp1_peak)
		b->results[b->view].vdp1_peak = ticks;
}

int bench_passed(const bench_run *b)
{
	for (int i = 0; i < bench_view_count; ++i)
		if (!b->results[i].pass && !bench_views[i].known)
			return 0;
	return 1;
}
