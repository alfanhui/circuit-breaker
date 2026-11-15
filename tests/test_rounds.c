#include "test.h"
#include "../src/game/rounds.h"
#include "../src/game/arena.h"
#include "../src/game/bot.h"

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

// (It went round to the first again, until the last round's rival was restarted: rounds.h)
static void the_last_round_doesnt_end(void)
{
	rounds r;
	int frames;

	rounds_init(&r);
	for (int round = 1; round < ROUND_COUNT; ++round)
	{
		rounds_update(&r, 0);
		until_event(&r, 0, 1000, &frames); // rises
		until_event(&r, 0, 1000, &frames); // begins
	}
	CHECK_EQ(ROUND_COUNT - 1, r.number);
	for (int frame = 0; frame < 2000; ++frame)
		rounds_update(&r, 0);
	CHECK_EQ(ROUND_COUNT - 1, r.number);
	CHECK_EQ(ROUND_PLAYING, r.phase);
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

// The last round can't be won: with no rival left it isn't cleared. The rival's restarted, a
// second and a half later, every time it dies; the earlier rounds end as they did
static void the_last_rounds_rival_is_restarted(void)
{
	rounds r;

	rounds_init(&r);
	r.number = ROUND_COUNT - 1;
	for (int again = 0; again < 3; ++again)
	{
		for (int frame = 0; frame < 200; ++frame)
			CHECK_EQ(ROUND_NOTHING, rounds_update(&r, 1)); // riding
		for (int frame = 1; frame < ROUND_RESTART_FRAMES; ++frame)
			CHECK_EQ(ROUND_NOTHING, rounds_update(&r, 0)); // dead: its trail sinks
		CHECK_EQ(ROUND_RESTARTS, rounds_update(&r, 0));
		CHECK_EQ(ROUND_PLAYING, r.phase);
		CHECK_EQ(ROUND_COUNT - 1, r.number);
	}
	// Dead only a moment (the restart, or a debug kill, then alive again): the count starts over
	for (int frame = 0; frame < ROUND_RESTART_FRAMES - 1; ++frame)
		rounds_update(&r, 0);
	rounds_update(&r, 1);
	for (int frame = 1; frame < ROUND_RESTART_FRAMES; ++frame)
		CHECK_EQ(ROUND_NOTHING, rounds_update(&r, 0));
	CHECK_EQ(ROUND_RESTARTS, rounds_update(&r, 0));
	// Any other round: cleared at once
	rounds_init(&r);
	CHECK_EQ(ROUND_CLEARED, rounds_update(&r, 0));
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


// Runs to the start of round `number` (0-based) being played
static void play_to_round(rounds *r, int number)
{
	rounds_init(r);
	while (r->number != number || r->phase != ROUND_PLAYING)
		rounds_update(r, 0);
}

static void each_round_has_its_layout(void)
{
	static const int expected[ROUND_COUNT] = {ARENA_OPEN, ARENA_DONUT, ARENA_PILLARS, ARENA_ROOMS, ARENA_OPEN};
	rounds r;

	for (int n = 0; n < ROUND_COUNT; ++n)
	{
		play_to_round(&r, n);
		CHECK_EQ(expected[n], rounds_layout(&r));
	}
}

// Only the last round closes in: still for half a beat, then a lunge, eased, then still again
static void the_last_round_closes_in_lunges(void)
{
	const int half_beat = 1 << (ROUND_LUNGE_SHIFT - 1);
	rounds r;
	int last;

	play_to_round(&r, 3);
	for (int i = 0; i < 5000; ++i)
		rounds_update(&r, 1);
	CHECK_EQ(0, rounds_closing(&r));
	CHECK_EQ(0, rounds_rumble(&r));

	play_to_round(&r, ROUND_COUNT - 1);
	// The first half beat: still, and quiet
	for (int i = 0; i < half_beat; ++i)
	{
		CHECK_EQ(0, rounds_closing(&r));
		CHECK_EQ(0, rounds_rumble(&r));
		rounds_update(&r, 1);
	}
	// The lunge: in all the way, never back out, never faster than a cycle rides (cruise 7), and
	// rumbling while it moves, hardest in the middle
	last = 0;
	for (int i = 0; i < half_beat; ++i)
	{
		int now = rounds_closing(&r);

		CHECK(now >= last);
		CHECK(now - last < 7);
		CHECK(rounds_rumble(&r) <= ROUND_RUMBLE_MAX);
		if (i == half_beat / 2)
			CHECK(rounds_rumble(&r) > ROUND_RUMBLE_MAX * 9 / 10);
		last = now;
		rounds_update(&r, 1);
	}
	CHECK_EQ(ROUND_LUNGE_DISTANCE, rounds_closing(&r)); // the next beat's still half
	CHECK_EQ(0, rounds_rumble(&r));
	// Long after: closed all the way, still, and held there (frames don't wrap round to 0)
	for (int i = 0; i < 70000; ++i)
		rounds_update(&r, 1);
	CHECK_EQ(ROUND_CLOSE_MAX, rounds_closing(&r));
	CHECK_EQ(0, rounds_rumble(&r));
	// Its rival dead: it isn't won (the rival's restarted, rounds.h), and stays closed
	rounds_update(&r, 0);
	CHECK_EQ(ROUND_CLOSE_MAX, rounds_closing(&r));
	// A restart opens it out again
	play_to_round(&r, ROUND_COUNT - 1);
	for (int i = 0; i < 1000; ++i)
		rounds_update(&r, 1);
	rounds_restart(&r);
	CHECK_EQ(0, rounds_closing(&r));
}

// The last two rounds' bots play at least Medium
static void the_end_of_the_loop_is_harder(void)
{
	static const int expected[ROUND_COUNT] = {BOT_EASY, BOT_EASY, BOT_EASY, BOT_MEDIUM, BOT_MEDIUM};
	rounds r;

	for (int n = 0; n < ROUND_COUNT; ++n)
	{
		play_to_round(&r, n);
		CHECK_EQ(expected[n], rounds_min_skill(&r));
	}
}

void run_rounds_tests(void)
{
	plays_while_a_rival_rides();
	the_walls_sink_lie_flat_then_rise();
	rivals_dont_matter_between_rounds();
	the_walls_move_one_way_at_a_time();
	the_last_round_doesnt_end();
	each_round_has_its_rivals_and_title();
	the_last_rounds_rival_is_restarted();
	a_restart_keeps_the_round();
	each_round_has_its_layout();
	the_last_round_closes_in_lunges();
	the_end_of_the_loop_is_harder();
}
