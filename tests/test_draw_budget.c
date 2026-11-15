// The draw budget (game/draw_budget.h, SG-8): what goes when VDP1 can't finish, and when it comes back
#include "test.h"
#include "../src/game/draw_budget.h"

// n frames in a row, all finished or all not
static void frames(draw_budget *b, int n, int unfinished)
{
	for (int i = 0; i < n; ++i)
		draw_budget_frame(b, unfinished);
}

static void everything_is_drawn_at_first(void)
{
	draw_budget b;

	draw_budget_init(&b);
	frames(&b, 500, 0);
	CHECK(draw_budget_allows(&b, BUDGET_LANDMARKS));
}

static void an_unfinished_frame_drops_a_part(void)
{
	draw_budget b;

	draw_budget_init(&b);
	draw_budget_frame(&b, 1);
	CHECK(!draw_budget_allows(&b, BUDGET_LANDMARKS));
	// Nothing more to drop: it stays at all of them out
	frames(&b, 5, 1);
	CHECK_EQ(BUDGET_PARTS, b.dropped);
}

static void a_part_comes_back_after_a_second_of_finished_frames(void)
{
	draw_budget b;

	draw_budget_init(&b);
	draw_budget_frame(&b, 1);
	frames(&b, BUDGET_HOLD_FRAMES - 1, 0);
	CHECK(!draw_budget_allows(&b, BUDGET_LANDMARKS)); // not yet
	draw_budget_frame(&b, 0);
	CHECK(draw_budget_allows(&b, BUDGET_LANDMARKS));
}

static void a_return_that_fails_waits_twice_as_long(void)
{
	draw_budget b;

	draw_budget_init(&b);
	draw_budget_frame(&b, 1);
	frames(&b, BUDGET_HOLD_FRAMES, 0); // back
	CHECK(draw_budget_allows(&b, BUDGET_LANDMARKS));
	frames(&b, 10, 0);
	draw_budget_frame(&b, 1); // it tipped VDP1 over: out again
	CHECK(!draw_budget_allows(&b, BUDGET_LANDMARKS));
	CHECK_EQ(2 * BUDGET_HOLD_FRAMES, b.hold);
	frames(&b, BUDGET_HOLD_FRAMES, 0);
	CHECK(!draw_budget_allows(&b, BUDGET_LANDMARKS)); // a second isn't enough now
	frames(&b, BUDGET_HOLD_FRAMES, 0);
	CHECK(draw_budget_allows(&b, BUDGET_LANDMARKS));
}

static void the_wait_stops_growing(void)
{
	draw_budget b;

	draw_budget_init(&b);
	draw_budget_frame(&b, 1);
	for (int i = 0; i < 20; ++i)
	{
		frames(&b, b.hold, 0); // back on trial...
		draw_budget_frame(&b, 1); // ...and fails at once
	}
	CHECK_EQ(BUDGET_HOLD_MAX, b.hold);
}

static void a_return_that_holds_shortens_the_wait_again(void)
{
	draw_budget b;

	draw_budget_init(&b);
	draw_budget_frame(&b, 1);
	frames(&b, BUDGET_HOLD_FRAMES, 0);
	draw_budget_frame(&b, 1); // failed: hold doubles
	frames(&b, 2 * BUDGET_HOLD_FRAMES, 0); // back again
	frames(&b, BUDGET_HOLD_FRAMES, 0); // and it held for a second
	CHECK_EQ(BUDGET_HOLD_FRAMES, b.hold);
}

static void a_miss_long_after_a_return_isnt_blamed_on_it(void)
{
	draw_budget b;

	draw_budget_init(&b);
	draw_budget_frame(&b, 1);
	frames(&b, BUDGET_HOLD_FRAMES, 0); // back
	frames(&b, 3 * BUDGET_HOLD_FRAMES, 0); // held well
	draw_budget_frame(&b, 1); // something else (a new view)
	CHECK_EQ(BUDGET_HOLD_FRAMES, b.hold);
	CHECK(!draw_budget_allows(&b, BUDGET_LANDMARKS));
}

void run_draw_budget_tests(void)
{
	RUN(everything_is_drawn_at_first);
	RUN(an_unfinished_frame_drops_a_part);
	RUN(a_part_comes_back_after_a_second_of_finished_frames);
	RUN(a_return_that_fails_waits_twice_as_long);
	RUN(the_wait_stops_growing);
	RUN(a_return_that_holds_shortens_the_wait_again);
	RUN(a_miss_long_after_a_return_isnt_blamed_on_it);
}
