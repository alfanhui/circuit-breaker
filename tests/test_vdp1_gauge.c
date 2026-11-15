#include "test.h"
#include "../src/game/vdp1_gauge.h"

// A frame as the registers would show it: did VDP1 finish the frame before (BEF), how long was
// that list (in commands), and was VDP1 done by the time the CPU was (CEF)
static void frame(vdp1_gauge *g, int finished_before, int commands, int done_with_cpu)
{
	vdp1_gauge_frame_start(g, (unsigned short)(finished_before ? VDP1_EDSR_BEF : 0), (unsigned short)(commands << 2));
	vdp1_gauge_frame_end(g, (unsigned short)(done_with_cpu ? VDP1_EDSR_CEF : 0));
}

static void starts_at_zero(void)
{
	vdp1_gauge g;

	vdp1_gauge_init(&g);
	vdp1_gauge_second(&g); // a second with no frames: no division by 0
	CHECK_EQ(0, g.over);
	CHECK_EQ(0, g.busy_percent);
	CHECK_EQ(0, g.commands);
}

static void an_easy_second_reads_all_clear(void)
{
	vdp1_gauge g;

	vdp1_gauge_init(&g);
	for (int i = 0; i < 60; ++i)
		frame(&g, 1, 300, 1);
	vdp1_gauge_second(&g);
	CHECK_EQ(0, g.over);
	CHECK_EQ(0, g.busy_percent);
	CHECK_EQ(300, g.commands);
}

// Frame by frame (the benchmark's count, game/bench.h): whether the frame before was finished
static void each_frame_says_if_the_one_before_was_finished(void)
{
	vdp1_gauge g;

	vdp1_gauge_init(&g);
	CHECK_EQ(0, g.last_unfinished);
	frame(&g, 0, 0, 1);
	CHECK_EQ(1, g.last_unfinished);
	frame(&g, 1, 100, 1);
	CHECK_EQ(0, g.last_unfinished);
	// ...and the total since boot keeps the one that wasn't, past the second's figures
	vdp1_gauge_second(&g);
	frame(&g, 0, 0, 1);
	CHECK_EQ(2, g.unfinished_total);
}

static void unfinished_frames_are_counted_and_not_measured(void)
{
	vdp1_gauge g;

	vdp1_gauge_init(&g);
	frame(&g, 1, 200, 1);
	frame(&g, 0, 900, 0); // unfinished: LOPR is only how far it got
	frame(&g, 0, 900, 0);
	vdp1_gauge_second(&g);
	CHECK_EQ(2, g.over);
	CHECK_EQ(200, g.commands); // the longest list it finished, not the half-drawn one
}

static void busy_is_the_share_of_frames_vdp1_outlasted_the_cpu(void)
{
	vdp1_gauge g;

	vdp1_gauge_init(&g);
	for (int i = 0; i < 60; ++i)
		frame(&g, 1, 100, i % 4 != 0); // 1 frame in 4 still drawing when the CPU was done
	vdp1_gauge_second(&g);
	CHECK_EQ(25, g.busy_percent);
}

static void each_second_starts_afresh(void)
{
	vdp1_gauge g;

	vdp1_gauge_init(&g);
	for (int i = 0; i < 30; ++i)
		frame(&g, 0, 500, 0);
	vdp1_gauge_second(&g);
	CHECK_EQ(30, g.over);
	CHECK_EQ(100, g.busy_percent);
	for (int i = 0; i < 60; ++i)
		frame(&g, 1, 120, 1);
	vdp1_gauge_second(&g);
	CHECK_EQ(0, g.over);
	CHECK_EQ(0, g.busy_percent);
	CHECK_EQ(120, g.commands);
}

static void other_status_bits_are_ignored(void)
{
	vdp1_gauge g;

	vdp1_gauge_init(&g);
	// Unused bits set (the manual says they read 0; don't rely on it): only BEF and CEF count
	vdp1_gauge_frame_start(&g, (unsigned short)(0xFFFC | VDP1_EDSR_BEF), 40);
	vdp1_gauge_frame_end(&g, (unsigned short)(0xFFFC | VDP1_EDSR_CEF));
	vdp1_gauge_second(&g);
	CHECK_EQ(0, g.over);
	CHECK_EQ(0, g.busy_percent);
	CHECK_EQ(10, g.commands);
}

void run_vdp1_gauge_tests(void)
{
	starts_at_zero();
	an_easy_second_reads_all_clear();
	each_frame_says_if_the_one_before_was_finished();
	unfinished_frames_are_counted_and_not_measured();
	busy_is_the_share_of_frames_vdp1_outlasted_the_cpu();
	each_second_starts_afresh();
	other_status_bits_are_ignored();
}
