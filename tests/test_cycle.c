#include "test.h"
#include "../src/game/cycle.h"
#include "../src/game/arena.h"
#include "../src/game/trail.h"

static void run_frames(cycle *c, int frames, int boosting)
{
	while (--frames >= 0)
		cycle_update(c, boosting, NULL, 0);
}

static void starts_slow_and_ramps_to_cruise(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_NORTH);
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(-2, c.position.y);
	run_frames(&c, 20, 0);
	CHECK_EQ(CYCLE_BASE_SPEED, c.speed);
}

static void moves_along_heading(void)
{
	cycle c;
	int start;

	cycle_init(&c, 0, 0, HEADING_NORTH);
	run_frames(&c, 10, 0);
	CHECK_EQ(0, c.position.x);
	CHECK(c.position.y < 0);

	cycle_turn_right(&c); // east
	start = c.position.x;
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(start - CYCLE_BASE_SPEED, c.position.x);

	cycle_turn_right(&c); // south
	start = c.position.y;
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(start + CYCLE_BASE_SPEED, c.position.y);

	cycle_turn_right(&c); // west
	start = c.position.x;
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(start + CYCLE_BASE_SPEED, c.position.x);
}

static void turns_wrap_around_the_compass(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_NORTH);
	cycle_turn_left(&c);
	CHECK_EQ(HEADING_WEST, c.heading);
	cycle_turn_right(&c);
	cycle_turn_right(&c);
	CHECK_EQ(HEADING_EAST, c.heading);
	cycle_turn_right(&c);
	cycle_turn_right(&c);
	cycle_turn_right(&c);
	CHECK_EQ(HEADING_NORTH, c.heading);
}

static void boost_speeds_up_and_drains_gauge(void)
{
	cycle c;
	int start;

	cycle_init(&c, 0, 0, HEADING_SOUTH);
	run_frames(&c, 10, 0);
	start = c.position.y;
	cycle_update(&c, 1, NULL, 0);
	CHECK(c.boost > 0); // felt on the first frame...
	CHECK(c.position.y - start > CYCLE_BASE_SPEED);
	run_frames(&c, 7, 1);
	CHECK_EQ(CYCLE_BOOST_FULL, c.boost); // ...built up, full in 8 frames
	CHECK_EQ(CYCLE_GAUGE_MAX - 8 * CYCLE_GAUGE_DRAIN, c.gauge);
}

static void boost_builds_up_rather_than_jumping(void)
{
	cycle c;
	int last = 0;

	cycle_init(&c, 0, 0, HEADING_SOUTH);
	// Each frame no more than an eighth of full: a snap to full looked like a glitch
	while (c.boost < CYCLE_BOOST_FULL)
	{
		cycle_update(&c, 1, NULL, 0);
		CHECK(c.boost - last <= CYCLE_BOOST_FULL / 8);
		last = c.boost;
	}
}

// How far one frame at full boost moves a cycle cruising at `speed`
static int full_boost_step(int speed)
{
	cycle c;
	int start;

	cycle_set_cruise_speed(speed);
	cycle_init(&c, 0, 0, HEADING_SOUTH);
	run_frames(&c, speed, 0); // ramp up to cruise
	run_frames(&c, CYCLE_BOOST_FULL / CYCLE_BOOST_RAMP_UP, 1); // boost to full
	start = c.position.y;
	cycle_update(&c, 1, NULL, 0);
	cycle_set_cruise_speed(CYCLE_BASE_SPEED); // cruise is shared by every cycle: put it back
	return c.position.y - start;
}

static void boost_is_the_same_share_at_any_speed(void)
{
	// +75% (12/16), to the nearest unit: the old fixed +7 was +100% at 7 but only +18% at 40
	CHECK_EQ(7 + 5, full_boost_step(7));
	CHECK_EQ(16 + 12, full_boost_step(16));
	CHECK_EQ(40 + 30, full_boost_step(40));
}

static void full_gauge_lasts_a_second_and_a_half(void)
{
	cycle c;
	int frames = 0;

	cycle_init(&c, 0, 0, HEADING_SOUTH);
	while (c.gauge >= CYCLE_GAUGE_DRAIN && frames < 1000)
	{
		cycle_update(&c, 1, NULL, 0);
		++frames;
	}
	CHECK_EQ(90, frames); // 1.5 s at 60 fps (it was 40 frames: 0.67 s)
}

static void boost_stops_when_gauge_empty(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_SOUTH);
	run_frames(&c, CYCLE_GAUGE_MAX / CYCLE_GAUGE_DRAIN, 1);
	CHECK_EQ(0, c.gauge);
	run_frames(&c, CYCLE_BOOST_FULL / CYCLE_BOOST_RAMP_DOWN, 1);
	CHECK_EQ(0, c.boost);
	run_frames(&c, 2 * CYCLE_REFILL_DELAY, 1);
	CHECK_EQ(0, c.gauge); // no refill while the button is held
}

static void boost_fades_after_release(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_SOUTH);
	run_frames(&c, CYCLE_BOOST_FULL / CYCLE_BOOST_RAMP_UP, 1);
	run_frames(&c, CYCLE_BOOST_FULL / CYCLE_BOOST_RAMP_DOWN - 1, 0);
	CHECK(c.boost > 0); // still fading...
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(0, c.boost); // ...gone 16 frames after letting go
}

static void gauge_waits_before_refilling(void)
{
	cycle c;
	int after_boost;

	cycle_init(&c, 0, 0, HEADING_SOUTH);
	run_frames(&c, 10, 1);
	after_boost = c.gauge;
	run_frames(&c, CYCLE_REFILL_DELAY, 0);
	CHECK_EQ(after_boost, c.gauge); // half a second of nothing...
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(after_boost + CYCLE_GAUGE_REFILL, c.gauge); // ...then it refills
	run_frames(&c, 1000, 0);
	CHECK_EQ(CYCLE_GAUGE_MAX, c.gauge); // and stops when full
}

static void tapping_never_refills(void)
{
	cycle c;
	int last;

	cycle_init(&c, 0, 0, HEADING_SOUTH);
	last = c.gauge;
	// Tap every 20 frames: each tap restarts the wait, so the gauge only goes down
	for (int tap = 0; tap < 10; ++tap)
	{
		cycle_update(&c, 1, NULL, 0);
		run_frames(&c, 19, 0);
		CHECK(c.gauge < last);
		last = c.gauge;
	}
}

static void stops_at_arena_limit(void)
{
	cycle c;
	cycle_init(&c, 0, (short)(arena_player_limit() - 3), HEADING_SOUTH);
	run_frames(&c, 10, 0);
	CHECK_EQ(arena_player_limit(), c.position.y);
	CHECK_EQ(1, cycle_update(&c, 0, NULL, 0));

	cycle_init(&c, (short)(-arena_player_limit() + 3), 0, HEADING_EAST);
	run_frames(&c, 10, 0);
	CHECK_EQ(-arena_player_limit(), c.position.x);
}

static void bounded_by_default(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_NORTH);
	CHECK_EQ(1, c.bounded);
}

static void unbounded_cycle_leaves_the_arena(void)
{
	cycle c;
	cycle_init(&c, 0, (short)(arena_player_limit() - 3), HEADING_SOUTH);
	c.bounded = 0;
	run_frames(&c, 10, 0);
	CHECK(c.position.y > arena_player_limit());
	CHECK_EQ(0, cycle_update(&c, 0, NULL, 0)); // nothing stopped it
}

static void unbounded_cycle_stops_before_short_overflow(void)
{
	cycle c;
	cycle_init(&c, WORLD_COORD_LIMIT - 3, 0, HEADING_WEST); // west is +x
	c.bounded = 0;
	run_frames(&c, 10, 1);
	CHECK_EQ(WORLD_COORD_LIMIT, c.position.x); // not wrapped round to a negative
	CHECK_EQ(1, cycle_update(&c, 0, NULL, 0));
}

static void rebounding_snaps_back_to_the_edge(void)
{
	cycle c;
	cycle_init(&c, 0, (short)(arena_player_limit() + 500), HEADING_SOUTH); // outside, as if the boundary was off
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(arena_player_limit(), c.position.y);
}

static void bounded_flag_costs_no_memory(void)
{
	// position (4) + heading, speed, boost, bounded (4) + gauge, refill_wait (2): the flag took a
	// padding byte, and the gauge (0..180) fits a byte, leaving room for refill_wait
	CHECK_EQ(10, sizeof(cycle));
}

// Integration: drive a cycle with turns and check the wall follows it exactly.
static void trail_follows_cycle_through_turns(void)
{
	cycle c;
	trail t;
	world_point from, to;
	world_point turn1, turn2;

	cycle_init(&c, 0, 0, HEADING_NORTH);
	trail_reset(&t, c.position);

	run_frames(&c, 20, 0);
	trail_set_head(&t, c.position);
	turn1 = c.position;
	cycle_turn_right(&c);
	trail_add_corner(&t);

	run_frames(&c, 15, 0);
	trail_set_head(&t, c.position);
	turn2 = c.position;
	cycle_turn_left(&c);
	trail_add_corner(&t);

	run_frames(&c, 5, 0);
	trail_set_head(&t, c.position);

	CHECK_EQ(3, trail_segment_count(&t));
	CHECK(trail_get_segment(&t, 0, 0, &from, &to));
	CHECK_EQ(turn1.y, to.y);
	CHECK(trail_get_segment(&t, 1, 0, &from, &to));
	CHECK_EQ(turn1.y, from.y);
	CHECK_EQ(turn2.x, to.x);
	CHECK_EQ(from.y, to.y); // east-west leg is horizontal
	CHECK(trail_get_segment(&t, 2, 0, &from, &to));
	CHECK_EQ(c.position.x, to.x);
	CHECK_EQ(c.position.y, to.y);
	CHECK_EQ(from.x, to.x); // north leg is vertical
}

void run_cycle_tests(void)
{
	RUN(starts_slow_and_ramps_to_cruise);
	RUN(moves_along_heading);
	RUN(turns_wrap_around_the_compass);
	RUN(boost_speeds_up_and_drains_gauge);
	RUN(boost_builds_up_rather_than_jumping);
	RUN(boost_is_the_same_share_at_any_speed);
	RUN(full_gauge_lasts_a_second_and_a_half);
	RUN(boost_stops_when_gauge_empty);
	RUN(boost_fades_after_release);
	RUN(gauge_waits_before_refilling);
	RUN(tapping_never_refills);
	RUN(stops_at_arena_limit);
	RUN(bounded_by_default);
	RUN(unbounded_cycle_leaves_the_arena);
	RUN(unbounded_cycle_stops_before_short_overflow);
	RUN(rebounding_snaps_back_to_the_edge);
	RUN(bounded_flag_costs_no_memory);
	RUN(trail_follows_cycle_through_turns);
}
