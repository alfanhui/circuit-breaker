#include "test.h"
#include "../src/game/cycle.h"
#include "../src/game/arena.h"
#include "../src/game/trail.h"

static void run_frames(cycle *c, int frames, int boosting)
{
	while (--frames >= 0)
		cycle_update(c, boosting);
}

static void starts_slow_and_ramps_to_cruise(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_NORTH);
	cycle_update(&c, 0);
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
	cycle_update(&c, 0);
	CHECK_EQ(start - CYCLE_BASE_SPEED, c.position.x);

	cycle_turn_right(&c); // south
	start = c.position.y;
	cycle_update(&c, 0);
	CHECK_EQ(start + CYCLE_BASE_SPEED, c.position.y);

	cycle_turn_right(&c); // west
	start = c.position.x;
	cycle_update(&c, 0);
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
	run_frames(&c, 2, 1);
	CHECK_EQ(CYCLE_BOOST_MAX, c.boost);
	CHECK_EQ(CYCLE_GAUGE_MAX - 2 * CYCLE_GAUGE_DRAIN, c.gauge);
	CHECK(c.position.y - start > 2 * CYCLE_BASE_SPEED);
}

static void boost_stops_when_gauge_empty(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_SOUTH);
	run_frames(&c, CYCLE_GAUGE_MAX / CYCLE_GAUGE_DRAIN, 1);
	CHECK_EQ(0, c.gauge);
	run_frames(&c, CYCLE_BOOST_MAX, 1);
	CHECK_EQ(0, c.boost);
	CHECK_EQ(0, c.gauge); // no refill while the button is held
}

static void boost_decays_and_gauge_refills_after_release(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_SOUTH);
	run_frames(&c, 4, 1);
	run_frames(&c, CYCLE_BOOST_MAX, 0);
	CHECK_EQ(0, c.boost);
	CHECK(c.gauge > CYCLE_GAUGE_MAX - 4 * CYCLE_GAUGE_DRAIN);
}

static void stops_at_arena_limit(void)
{
	cycle c;
	cycle_init(&c, 0, (short)(arena_player_limit() - 3), HEADING_SOUTH);
	run_frames(&c, 10, 0);
	CHECK_EQ(arena_player_limit(), c.position.y);
	CHECK_EQ(1, cycle_update(&c, 0));

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
	CHECK_EQ(0, cycle_update(&c, 0)); // nothing stopped it
}

static void unbounded_cycle_stops_before_short_overflow(void)
{
	cycle c;
	cycle_init(&c, WORLD_COORD_LIMIT - 3, 0, HEADING_WEST); // west is +x
	c.bounded = 0;
	run_frames(&c, 10, 1);
	CHECK_EQ(WORLD_COORD_LIMIT, c.position.x); // not wrapped round to a negative
	CHECK_EQ(1, cycle_update(&c, 0));
}

static void rebounding_snaps_back_to_the_edge(void)
{
	cycle c;
	cycle_init(&c, 0, (short)(arena_player_limit() + 500), HEADING_SOUTH); // outside, as if the boundary was off
	cycle_update(&c, 0);
	CHECK_EQ(arena_player_limit(), c.position.y);
}

static void bounded_flag_costs_no_memory(void)
{
	// position (4) + heading, speed, boost, bounded (4) + gauge (2): the flag took a padding byte
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
	RUN(boost_stops_when_gauge_empty);
	RUN(boost_decays_and_gauge_refills_after_release);
	RUN(stops_at_arena_limit);
	RUN(bounded_by_default);
	RUN(unbounded_cycle_leaves_the_arena);
	RUN(unbounded_cycle_stops_before_short_overflow);
	RUN(rebounding_snaps_back_to_the_edge);
	RUN(bounded_flag_costs_no_memory);
	RUN(trail_follows_cycle_through_turns);
}
