#include "test.h"
#include "../src/game/collision.h"
#include "../src/game/cycle.h"
#include "../src/game/arena.h"

// Remember the directions: north is -y, east is -x, south is +y, west is +x (world.h)

static world_point pt(short x, short y)
{
	world_point p;
	p.x = x;
	p.y = y;
	return p;
}

static void trail_across_the_path_stops_the_cycle_short(void)
{
	// Heading north from y = 0 towards a trail running east-west at y = -100: the cycle stops
	// clearance short of it, at y = -75, so it's 75 away
	CHECK_EQ(75, collision_segment(pt(0, 0), HEADING_NORTH, 200, pt(-500, -100), pt(500, -100), 25));
}

static void trail_out_of_reach_this_frame_does_not_stop_it(void)
{
	CHECK_EQ(7, collision_segment(pt(0, 0), HEADING_NORTH, 7, pt(-500, -100), pt(500, -100), 25));
	// Landing exactly on the edge only touches it: next frame's move is the crash
	CHECK_EQ(75, collision_segment(pt(0, 0), HEADING_NORTH, 75, pt(-500, -100), pt(500, -100), 25));
	CHECK_EQ(0, collision_segment(pt(0, -75), HEADING_NORTH, 7, pt(-500, -100), pt(500, -100), 25));
}

static void trail_behind_does_not_stop_it(void)
{
	CHECK_EQ(200, collision_segment(pt(0, 0), HEADING_SOUTH, 200, pt(-500, -100), pt(500, -100), 25));
}

static void trail_beside_the_path_does_not_stop_it(void)
{
	// The trail ends at x = 100; a cycle at x = 200 passes well clear of its end
	CHECK_EQ(200, collision_segment(pt(200, 0), HEADING_NORTH, 200, pt(-500, -100), pt(100, -100), 25));
	// Exactly clearance from the end slides past...
	CHECK_EQ(200, collision_segment(pt(125, 0), HEADING_NORTH, 200, pt(-500, -100), pt(100, -100), 25));
	// ...one unit closer clips it
	CHECK_EQ(75, collision_segment(pt(124, 0), HEADING_NORTH, 200, pt(-500, -100), pt(100, -100), 25));
}

static void head_on_into_the_end_of_a_trail_stops_it(void)
{
	// Driving west (+x) straight at the end of a trail lying on the same line
	CHECK_EQ(75, collision_segment(pt(0, 0), HEADING_WEST, 200, pt(100, 0), pt(900, 0), 25));
}

static void every_heading_stops_the_same_distance_short(void)
{
	// A square of trail around the origin, 100 out on every side: whichever way it goes, the
	// cycle stops 75 out
	CHECK_EQ(75, collision_segment(pt(0, 0), HEADING_NORTH, 200, pt(-100, -100), pt(100, -100), 25));
	CHECK_EQ(75, collision_segment(pt(0, 0), HEADING_EAST, 200, pt(-100, -100), pt(-100, 100), 25));
	CHECK_EQ(75, collision_segment(pt(0, 0), HEADING_SOUTH, 200, pt(100, 100), pt(-100, 100), 25));
	CHECK_EQ(75, collision_segment(pt(0, 0), HEADING_WEST, 200, pt(100, 100), pt(100, -100), 25));
}

static void own_live_segment_never_stops_it(void)
{
	trail t;

	// The trail grows from the cycle: it's always inside its own live segment
	trail_reset(&t, pt(0, 1000));
	trail_set_head(&t, pt(0, 0));
	CHECK_EQ(7, collision_trail(pt(0, 0), HEADING_NORTH, 7, &t));
}

static void segment_just_finished_does_not_stop_a_turn(void)
{
	trail t;

	// Came north to the origin and turned east: the cycle is still within 25 of the segment it
	// just laid, and driving out of it
	trail_reset(&t, pt(0, 1000));
	trail_set_head(&t, pt(0, 0));
	trail_add_corner(&t);
	CHECK_EQ(7, collision_trail(pt(0, 0), HEADING_EAST, 7, &t));
}

static void u_turn_runs_back_beside_its_trail(void)
{
	trail t;

	// North to the origin, east 50 (the turn gate's gap), then south: back alongside, 50 away
	trail_reset(&t, pt(0, 1000));
	trail_set_head(&t, pt(0, 0));
	trail_add_corner(&t);
	trail_set_head(&t, pt(-50, 0));
	trail_add_corner(&t);
	CHECK_EQ(7, collision_trail(pt(-50, 0), HEADING_SOUTH, 7, &t));
	CHECK_EQ(7, collision_trail(pt(-50, 500), HEADING_SOUTH, 7, &t));
}

static void turning_back_into_its_own_trail_stops_it(void)
{
	trail t;

	// North 1000, then west 300, then south, then east, back towards the first segment
	trail_reset(&t, pt(0, 1000));
	trail_set_head(&t, pt(0, 0));
	trail_add_corner(&t);
	trail_set_head(&t, pt(300, 0));
	trail_add_corner(&t);
	trail_set_head(&t, pt(300, 500));
	trail_add_corner(&t);
	// Heading east (-x) from x = 300: the first segment is at x = 0, so it stops at x = 25
	CHECK_EQ(275, collision_trail(pt(300, 500), HEADING_EAST, 400, &t));
}

// Stopped, a cycle can turn twice on the spot (turn_gate lets a stopped cycle always turn): it
// then faces back down the trail it just laid, and is inside that segment's fattened area
static void cannot_turn_round_onto_its_own_trail(void)
{
	trail t;

	// Came north 1000 to the origin and stopped there; turned right twice: now facing south
	trail_reset(&t, pt(0, 1000));
	trail_set_head(&t, pt(0, 0));
	trail_add_corner(&t);
	trail_add_corner(&t); // the second turn, without moving: an empty segment
	CHECK_EQ(0, collision_trail(pt(0, 0), HEADING_SOUTH, 7, &t));
}

static void crash_then_jink_cannot_backtrack(void)
{
	trail other;
	trail own;
	const trail *both[2];

	// A trail running north-south at x = 0. Ours came west (+x) along y = 0 and crashed 25 short
	trail_reset(&other, pt(0, 1000));
	trail_set_head(&other, pt(0, -1000));
	trail_reset(&own, pt(-500, 0));
	trail_set_head(&own, pt(-25, 0));
	both[0] = &other;
	both[1] = &own;
	// Left (south) a bit...
	trail_add_corner(&own);
	CHECK_EQ(7, collision_free_distance(pt(-25, 0), HEADING_SOUTH, 7, 10000, both, 2));
	trail_set_head(&own, pt(-25, 30));
	// ...right (west) into the same trail again: stopped straight away...
	trail_add_corner(&own);
	CHECK_EQ(0, collision_free_distance(pt(-25, 30), HEADING_WEST, 7, 10000, both, 2));
	// ...and right again (north), back up the 30 it just laid: that's its own trail, a crash too
	trail_add_corner(&own);
	CHECK_EQ(0, collision_free_distance(pt(-25, 30), HEADING_NORTH, 7, 10000, both, 2));
	// Away from both (south, along the trail it hit) is still free
	CHECK_EQ(7, collision_free_distance(pt(-25, 30), HEADING_SOUTH, 7, 10000, both, 2));
}

static void edge_stops_it_at_the_limit(void)
{
	CHECK_EQ(7, collision_edge(pt(0, 0), HEADING_SOUTH, 7, 1000));
	CHECK_EQ(3, collision_edge(pt(0, 997), HEADING_SOUTH, 7, 1000));
	CHECK_EQ(3, collision_edge(pt(-997, 0), HEADING_EAST, 7, 1000));
	CHECK_EQ(0, collision_edge(pt(0, 1000), HEADING_SOUTH, 7, 1000));
	CHECK_EQ(0, collision_edge(pt(0, 1500), HEADING_SOUTH, 7, 1000)); // outside: no further
	CHECK_EQ(7, collision_edge(pt(0, 1000), HEADING_NORTH, 7, 1000)); // free to head back in
}

static void nearest_obstacle_wins(void)
{
	trail near_trail;
	trail far_trail;
	const trail *both[2];

	trail_reset(&near_trail, pt(-500, -100));
	trail_set_head(&near_trail, pt(500, -100));
	trail_reset(&far_trail, pt(-500, -300));
	trail_set_head(&far_trail, pt(500, -300));
	// Either order: the edge at 1000, a trail at 300 and one at 100 => stopped at 75
	both[0] = &far_trail;
	both[1] = &near_trail;
	CHECK_EQ(75, collision_free_distance(pt(0, 0), HEADING_NORTH, 2000, 1000, both, 2));
	both[0] = &near_trail;
	both[1] = &far_trail;
	CHECK_EQ(75, collision_free_distance(pt(0, 0), HEADING_NORTH, 2000, 1000, both, 2));
	CHECK_EQ(1000, collision_free_distance(pt(0, 0), HEADING_NORTH, 2000, 1000, 0, 0));
}

// The cycle side: what a crash does to it

static void crash_stops_the_cycle(void)
{
	cycle c;
	trail t;
	const trail *trails[1];

	trails[0] = &t;
	trail_reset(&t, pt(-500, -100));
	trail_set_head(&t, pt(500, -100));
	cycle_init(&c, 0, -70, HEADING_NORTH);
	c.speed = CYCLE_BASE_SPEED;
	c.boost = CYCLE_BOOST_FULL;
	CHECK_EQ(1, cycle_update(&c, 0, trails, 1));
	CHECK_EQ(-75, c.position.y); // went the last 5 units, no further
	CHECK_EQ(0, c.speed);
	CHECK_EQ(0, c.boost);
	// Facing the trail it stays put
	CHECK_EQ(1, cycle_update(&c, 0, trails, 1));
	CHECK_EQ(-75, c.position.y);
}

static void crashed_cycle_builds_back_up_after_turning(void)
{
	cycle c;
	trail t;
	const trail *trails[1];

	trails[0] = &t;
	trail_reset(&t, pt(-500, -100));
	trail_set_head(&t, pt(500, -100));
	cycle_init(&c, 0, -75, HEADING_NORTH);
	cycle_update(&c, 0, trails, 1);
	cycle_turn_right(&c); // east, along the trail
	CHECK_EQ(0, cycle_update(&c, 0, trails, 1));
	CHECK_EQ(1, c.speed); // from a standing start
	CHECK_EQ(-1, c.position.x);
}

static void ghost_cycle_passes_through_trails(void)
{
	cycle c;
	trail t;
	const trail *trails[1];

	trails[0] = &t;
	trail_reset(&t, pt(-500, -100));
	trail_set_head(&t, pt(500, -100));
	cycle_init(&c, 0, -70, HEADING_NORTH);
	c.bounded = 0; // debug (Y)
	c.speed = CYCLE_BASE_SPEED;
	CHECK_EQ(0, cycle_update(&c, 0, trails, 1));
	CHECK_EQ(-77, c.position.y);
}

static void arena_edge_is_a_crash_too(void)
{
	cycle c;

	cycle_init(&c, 0, (short)(arena_player_limit() - 3), HEADING_SOUTH);
	c.speed = CYCLE_BASE_SPEED;
	CHECK_EQ(1, cycle_update(&c, 0, NULL, 0));
	CHECK_EQ(arena_player_limit(), c.position.y);
	CHECK_EQ(0, c.speed);
}

void run_collision_tests(void)
{
	RUN(trail_across_the_path_stops_the_cycle_short);
	RUN(trail_out_of_reach_this_frame_does_not_stop_it);
	RUN(trail_behind_does_not_stop_it);
	RUN(trail_beside_the_path_does_not_stop_it);
	RUN(head_on_into_the_end_of_a_trail_stops_it);
	RUN(every_heading_stops_the_same_distance_short);
	RUN(own_live_segment_never_stops_it);
	RUN(segment_just_finished_does_not_stop_a_turn);
	RUN(u_turn_runs_back_beside_its_trail);
	RUN(turning_back_into_its_own_trail_stops_it);
	RUN(cannot_turn_round_onto_its_own_trail);
	RUN(crash_then_jink_cannot_backtrack);
	RUN(edge_stops_it_at_the_limit);
	RUN(nearest_obstacle_wins);
	RUN(crash_stops_the_cycle);
	RUN(crashed_cycle_builds_back_up_after_turning);
	RUN(ghost_cycle_passes_through_trails);
	RUN(arena_edge_is_a_crash_too);
}
