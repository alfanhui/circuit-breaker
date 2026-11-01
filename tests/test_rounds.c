#include "test.h"
#include "../src/game/rounds.h"

// Runs frames until an event other than ROUND_NOTHING, or `max` frames. Returns the event
static int until_event(rounds *r, int rivals_alive, int max, int *frames)
{
	int event = ROUND_NOTHING;

	*frames = 0;
	while (event == ROUND_NOTHING && *frames < max)
	{
		event = rounds_update(r, rivals_alive);
		(*frames)++;
	}
	return event;
}

static void plays_while_a_rival_rides(void)
{
	rounds r;

	rounds_init(&r);
	for (int i = 0; i < 1000; ++i)
		CHECK_EQ(ROUND_NOTHING, rounds_update(&r, 1));
	CHECK_EQ(ROUND_PLAYING, r.phase);
	CHECK_EQ(0, r.number);
	CHECK_EQ(ROUND_WALLS_FULL, rounds_wall_rise(&r));
}

static void the_walls_sink_lie_flat_then_rise(void)
{
	rounds r;
	int frames;

	rounds_init(&r);
	CHECK_EQ(ROUND_CLEARED, rounds_update(&r, 0)); // the last rival's gone
	CHECK_EQ(ROUND_SINKING, r.phase);
	for (int i = 0; i < (1 << ROUND_SINK_SHIFT); ++i)
		rounds_update(&r, 0);
	CHECK_EQ(ROUND_OPEN, r.phase);
	CHECK_EQ(0, rounds_wall_rise(&r));
	// Open for a while, then the walls start up (the moment to move the world), then they're up
	CHECK_EQ(ROUND_RISES, until_event(&r, 0, 1000, &frames));
	CHECK_EQ(ROUND_OPEN_FRAMES, frames);
	CHECK_EQ(0, rounds_wall_rise(&r)); // not yet risen at all: the move can't be seen
	CHECK_EQ(ROUND_BEGINS, until_event(&r, 0, 1000, &frames));
	CHECK_EQ(1 << ROUND_RISE_SHIFT, frames);
	CHECK_EQ(ROUND_PLAYING, r.phase);
	CHECK_EQ(1, r.number);
	CHECK_EQ(ROUND_WALLS_FULL, rounds_wall_rise(&r));
}

// No rivals between rounds: nothing else may end the interlude early
static void rivals_dont_matter_between_rounds(void)
{
	rounds r;
	int frames;

	rounds_init(&r);
	rounds_update(&r, 0);
	CHECK_EQ(ROUND_RISES, until_event(&r, 3, 1000, &frames));
	CHECK_EQ(ROUND_BEGINS, until_event(&r, 3, 1000, &frames));
}

// While sinking the walls only go down, and while rising only up: no jolts back
static void the_walls_move_one_way_at_a_time(void)
{
	rounds r;
	int last = ROUND_WALLS_FULL, ok = 1;

	rounds_init(&r);
	rounds_update(&r, 0);
	while (r.phase == ROUND_SINKING)
	{
		int now = rounds_wall_rise(&r);

		ok &= now <= last;
		last = now;
		rounds_update(&r, 0);
	}
	CHECK(ok);
	while (r.phase != ROUND_RISING)
		rounds_update(&r, 0);
	last = 0;
	while (r.phase == ROUND_RISING)
	{
		int now = rounds_wall_rise(&r);

		ok &= now >= last;
		last = now;
		rounds_update(&r, 0);
	}
	CHECK(ok);
}

static void after_the_last_round_the_loop_goes_round(void)
{
	rounds r;
	int frames;

	rounds_init(&r);
	for (int round = 1; round <= ROUND_COUNT; ++round)
	{
		rounds_update(&r, 0);
		until_event(&r, 0, 1000, &frames); // rises
		until_event(&r, 0, 1000, &frames); // begins
	}
	CHECK_EQ(0, r.number);
}

static void each_round_has_its_rivals_and_title(void)
{
	rounds r;
	int frames;

	rounds_init(&r);
	CHECK_EQ(1, rounds_rivals(&r));
	CHECK(rounds_title(&r)[15] == '1');
	rounds_update(&r, 0);
	until_event(&r, 0, 1000, &frames);
	until_event(&r, 0, 1000, &frames);
	CHECK_EQ(2, rounds_rivals(&r));
	CHECK(rounds_title(&r)[15] == '2');
}

// A crash mid-round: the terminal screen starts the same round again
static void a_restart_keeps_the_round(void)
{
	rounds r;
	int frames;

	rounds_init(&r);
	rounds_update(&r, 0);
	until_event(&r, 0, 1000, &frames);
	until_event(&r, 0, 1000, &frames);
	rounds_restart(&r);
	CHECK_EQ(1, r.number);
	CHECK_EQ(ROUND_PLAYING, r.phase);
	CHECK_EQ(ROUND_WALLS_FULL, rounds_wall_rise(&r));
}

void run_rounds_tests(void)
{
	plays_while_a_rival_rides();
	the_walls_sink_lie_flat_then_rise();
	rivals_dont_matter_between_rounds();
	the_walls_move_one_way_at_a_time();
	after_the_last_round_the_loop_goes_round();
	each_round_has_its_rivals_and_title();
	a_restart_keeps_the_round();
}
