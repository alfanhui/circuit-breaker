#include "test.h"
#include "../src/game/bench.h"

/*
 * Steps a run until its view moves on, with the same figures every frame; counts the actions.
 * The call that moves it on is also the next view's first (its BENCH_PLACE): the figures a frame
 * passes in are the frame before's, so the last measured frame's arrive then
 */
static int finish_view(bench_run *b, int late, int unfinished, int cpu, int *places, int *measures)
{
	const int view = b->view;
	int calls = 0;

	while (b->view == view)
	{
		bench_action a = bench_frame(b, late, unfinished, cpu);

		calls++;
		if (b->view == view)
		{
			*places += a == BENCH_PLACE;
			*measures += a == BENCH_MEASURE;
		}
	}
	return calls;
}

// Each view: placed once, on its first frame, then settles, then is measured for its frames
static void each_view_is_placed_once_then_measured(void)
{
	bench_run b;
	int places = 0, measures = 0;

	bench_init(&b);
	CHECK_EQ(BENCH_SETTLE_FRAMES + BENCH_MEASURE_FRAMES + 1, finish_view(&b, 0, 0, 50, &places, &measures));
	CHECK_EQ(1, places);
	CHECK_EQ(BENCH_MEASURE_FRAMES, measures);
	CHECK_EQ(1, b.view);
	CHECK(b.results[0].pass);
	CHECK_EQ(50, b.results[0].peak);
}

// What settling costs isn't counted: a slow move to a new place mustn't fail the view
static void the_settle_isnt_counted(void)
{
	bench_run b;
	int places = 0, measures = 0;

	bench_init(&b);
	CHECK_EQ(BENCH_PLACE, bench_frame(&b, 0, 0, 0));
	for (int i = 0; i < BENCH_SETTLE_FRAMES; ++i)
		bench_frame(&b, 3, 1, 200); // slow, while settling
	finish_view(&b, 0, 0, 40, &places, &measures);
	CHECK_EQ(0, b.results[0].late);
	CHECK_EQ(0, b.results[0].over);
	CHECK_EQ(40, b.results[0].peak);
	CHECK(b.results[0].pass);
}

// Exactly BENCH_MEASURE_FRAMES frames' figures are counted
static void every_measured_frame_counts(void)
{
	bench_run b;
	int places = 0, measures = 0;

	bench_init(&b);
	finish_view(&b, 1, 1, 120, &places, &measures);
	CHECK_EQ(BENCH_MEASURE_FRAMES, b.results[0].late);
	CHECK_EQ(BENCH_MEASURE_FRAMES, b.results[0].over);
	CHECK_EQ(120, b.results[0].peak);
	CHECK(!b.results[0].pass);
}

// One late frame or one unfinished one fails a view, and the run
static void one_bad_frame_fails(void)
{
	bench_run b;
	int places = 0, measures = 0;

	bench_init(&b);
	for (int i = 0; i < BENCH_SETTLE_FRAMES + 10; ++i)
		bench_frame(&b, 0, 0, 30);
	bench_frame(&b, 0, 1, 30); // one frame VDP1 didn't finish
	finish_view(&b, 0, 0, 30, &places, &measures);
	CHECK_EQ(1, b.results[0].over);
	CHECK_EQ(0, b.results[0].late);
	CHECK(!b.results[0].pass);
	while (bench_frame(&b, 0, 0, 30) != BENCH_DONE)
		;
	CHECK(bench_views[0].known || !bench_passed(&b));
}

// All views run, in order, then it's done for good; a clean run passes
static void a_clean_run_passes(void)
{
	bench_run b;
	int frames = 0;

	bench_init(&b);
	while (bench_frame(&b, 0, 0, 30) != BENCH_DONE)
		frames++;
	// Each view's frames, and the last's figures arrive with the call that says it's done
	CHECK_EQ(bench_view_count * (BENCH_SETTLE_FRAMES + BENCH_MEASURE_FRAMES), frames);
	CHECK_EQ(bench_view_count, b.view);
	CHECK_EQ(BENCH_DONE, bench_frame(&b, 0, 0, 30));
	CHECK(bench_passed(&b));
	for (int i = 0; i < bench_view_count; ++i)
		CHECK(b.results[i].pass);
}

// VDP1's time is only noted while a view's counted, and only its longest
static void vdp1_time_is_the_measured_peak(void)
{
	bench_run b;

	bench_init(&b);
	for (int i = 0; i <= BENCH_SETTLE_FRAMES; ++i)
	{
		bench_frame(&b, 0, 0, 30);
		bench_note_vdp1(&b, 20000); // settling: not counted
	}
	CHECK_EQ(0, b.results[0].vdp1_peak);
	bench_frame(&b, 0, 0, 30);
	bench_note_vdp1(&b, 9000);
	bench_frame(&b, 0, 0, 30);
	bench_note_vdp1(&b, 12000);
	bench_frame(&b, 0, 0, 30);
	bench_note_vdp1(&b, 11000);
	CHECK_EQ(12000, b.results[0].vdp1_peak);
	CHECK(b.results[0].pass || b.view == 0); // VDP1's time doesn't judge it
}

// The table fits the results and the verdict's lines
static void the_views_fit(void)
{
	CHECK(bench_view_count > 0);
	CHECK(bench_view_count <= BENCH_VIEWS_MAX);
	for (int i = 0; i < bench_view_count; ++i)
	{
		int n = 0;

		while (bench_views[i].name[n] != 0)
			n++;
		CHECK(n <= 12);
		CHECK(bench_views[i].quarter < 4);
	}
}

void run_bench_tests(void)
{
	RUN(each_view_is_placed_once_then_measured);
	RUN(the_settle_isnt_counted);
	RUN(every_measured_frame_counts);
	RUN(one_bad_frame_fails);
	RUN(a_clean_run_passes);
	RUN(vdp1_time_is_the_measured_peak);
	RUN(the_views_fit);
}
