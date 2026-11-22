#include "test.h"
#include "../src/game/drone.h"
#include "../src/game/world.h"

// Two stops 2000 apart along x, 120 apart in height, both looking west (+x): waits 40 each
static const drone_stop two_stops[2] = {
	{0, 0, 0, HEADING_WEST, 40},
	{2000, 0, -120, HEADING_WEST, 40},
};

static void starts_waiting_at_its_first_stop(void)
{
	drone d;

	drone_init(&d, two_stops, 2, true, 1);
	CHECK(d.waiting);
	CHECK_EQ(0, d.at);
	CHECK_EQ(0, d.z);
	CHECK_EQ(HEADING_WEST << 14, d.yaw);
	CHECK_EQ(HEADING_WEST, drone_settled_heading(&d));
}

// Its forward is a unit vector (x1024) the way it faces: west is +x, north -y (world.h)
static void faces_the_way_its_yaw_says(void)
{
	drone d;
	int fx, fy;

	drone_init(&d, two_stops, 2, true, 1);
	drone_forward(&d, &fx, &fy);
	CHECK_EQ(1024, fx);
	CHECK_EQ(0, fy);
	d.yaw = HEADING_NORTH << 14;
	drone_forward(&d, &fx, &fy);
	CHECK_EQ(0, fx);
	CHECK_EQ(-1024, fy);
}

/*
 * It waits, flies to the next stop and gets there: never faster a frame than its fastest leg (and
 * its weave), its height easing at DRONE_CLIMB; then it's waiting there, exactly on it
 */
static void flies_to_its_next_stop_and_arrives(void)
{
	drone d;
	int frames = 0;

	drone_init(&d, two_stops, 2, true, 1);
	while (d.waiting && frames < 100)
	{
		drone_update(&d, 0, 0);
		++frames;
	}
	CHECK(!d.waiting);
	CHECK_EQ(1, d.at); // back and forth from the first stop: on to the second, whichever way it chose
	frames = 0;
	while (!d.waiting && frames < 2000)
	{
		const int x = d.x, y = d.y, z = d.z;

		drone_update(&d, 0, 0);
		if (!d.waiting)
		{
			CHECK((d.x - x) * (d.x - x) + (d.y - y) * (d.y - y) <= 7 * 7);
			CHECK(d.z - z >= -DRONE_CLIMB && d.z - z <= DRONE_CLIMB);
		}
		++frames;
	}
	CHECK(d.waiting);
	CHECK_EQ(2000, d.x);
	CHECK_EQ(0, d.y);
	// 2000 at 2.5 to 4.5 a frame, and a hesitation or two: well within 2000 frames, not under 400
	CHECK(frames > 400 && frames < 2000);
}

// Its stop behind it: it turns on the spot, at its fastest, before it moves on
static void turns_on_the_spot_to_a_stop_behind_it(void)
{
	static const drone_stop behind[2] = {{0, 0, 0, HEADING_WEST, 1}, {-2000, 0, 0, HEADING_EAST, 40}};
	drone d;

	drone_init(&d, behind, 2, true, 1);
	drone_update(&d, 0, 0); // its wait's over (a frame or less): off it goes, the stop east of it
	CHECK(!d.waiting);
	for (int i = 0; i < 20; ++i)
	{
		const int yaw = d.yaw;
		const bool hesitating = d.pause > 0; // then it hangs, not turning either
		int turned;

		drone_update(&d, 0, 0);
		turned = (short)(unsigned short)(d.yaw - yaw);
		CHECK_EQ(0, d.x);
		CHECK_EQ(0, d.y);
		CHECK_EQ(hesitating ? 0 : DRONE_TURN_MAX, turned < 0 ? -turned : turned);
	}
}

// Runs a drone `legs` stops on, writing each stop it reaches
static void stops_reached(drone *d, int legs, int *reached)
{
	for (int leg = 0; leg < legs; ++leg)
	{
		int frames = 0;

		while (d->waiting && frames++ < 1000)
			drone_update(d, 0, 0);
		while (!d->waiting && frames++ < 4000)
			drone_update(d, 0, 0);
		reached[leg] = d->waiting ? d->at : -1;
	}
}

// A square of four stops 600 apart, round which a loop goes
static const drone_stop square[4] = {
	{0, 0, 300, HEADING_WEST, 30},
	{600, 0, 300, HEADING_SOUTH, 30},
	{600, 600, 300, HEADING_EAST, 30},
	{0, 600, 300, HEADING_NORTH, 30},
};

/*
 * A loop goes to a neighbouring stop each time, round past the end. Now and then it turns back
 * and goes round the other way (a quarter of its stops: it keeps on that way till it turns again)
 */
static void a_loop_goes_round_and_sometimes_back(void)
{
	drone d;
	int reached[60], from = 0, last_step = 1, turns = 0;

	drone_init(&d, square, 4, false, 7);
	stops_reached(&d, 60, reached);
	for (int leg = 0; leg < 60; ++leg)
	{
		const int step = (reached[leg] - from + 4) & 3;

		CHECK(reached[leg] >= 0); // got there
		CHECK(step == 1 || step == 3);
		turns += step != last_step;
		last_step = step;
		from = reached[leg];
	}
	CHECK(turns > 3);  // it does turn back
	CHECK(turns < 30); // but mostly carries on round
}

// Back and forth: always a neighbour, never round from one end to the other
static void back_and_forth_turns_back_at_its_ends(void)
{
	drone d;
	int reached[60], from = 0;

	drone_init(&d, square, 3, true, 11);
	stops_reached(&d, 60, reached);
	for (int leg = 0; leg < 60; ++leg)
	{
		CHECK(reached[leg] >= 0);
		CHECK(reached[leg] - from == 1 || reached[leg] - from == -1);
		from = reached[leg];
	}
}

// Each wait is half to one and a half times the stop's, and they differ
static void waits_vary(void)
{
	drone d;
	int shortest = 1000, longest = 0;

	drone_init(&d, square, 4, false, 3);
	for (int leg = 0; leg < 40; ++leg)
	{
		int waited = 0, frames = 0;

		while (!d.waiting && frames++ < 4000)
			drone_update(&d, 0, 0);
		while (d.waiting)
		{
			drone_update(&d, 0, 0);
			++waited;
		}
		if (leg == 0)
			continue; // the first wait is the route's own, at its first stop
		if (waited < shortest)
			shortest = waited;
		if (waited > longest)
			longest = waited;
	}
	CHECK(shortest >= 15 && longest < 45);
	CHECK(longest > shortest);
}

// The same seed flies the same patrol, frame for frame; another seed another
static void the_same_seed_flies_the_same_patrol(void)
{
	drone a, b, c;
	bool apart = false;

	drone_init(&a, square, 4, false, 42);
	drone_init(&b, square, 4, false, 42);
	drone_init(&c, square, 4, false, 43);
	for (int i = 0; i < 3000; ++i)
	{
		drone_update(&a, 0, 0);
		drone_update(&b, 0, 0);
		drone_update(&c, 0, 0);
		CHECK_EQ(a.x, b.x);
		CHECK_EQ(a.y, b.y);
		CHECK_EQ(a.yaw, b.yaw);
		apart |= a.x != c.x || a.y != c.y;
	}
	CHECK(apart);
}

// The drone up at (0, 0, 300), facing west (+x): its sensor 140 ahead of it, at x 140, 290 up. No
// roofs: its beam's square stays DRONE_BEAM_DROP under it, at -200
static const drone_stop high_west[1] = {{0, 0, 300, HEADING_WEST, 60000}};
#define SQUARE_MIDDLE (140 + DRONE_BEAM_AHEAD) // the square's middle, x

// The beam's base: a square DRONE_BEAM_HALF either way of a point DRONE_BEAM_AHEAD ahead
static void the_beam_is_a_square_ahead_and_below(void)
{
	drone d;
	drone_beam b;

	drone_init(&d, high_west, 1, false, 1);
	drone_beam_get(&d, &b);
	CHECK_EQ(140, b.apex.x);
	CHECK_EQ(290, b.apex.z);
	CHECK_EQ(SQUARE_MIDDLE - DRONE_BEAM_HALF, b.base[0].x); // near left
	CHECK_EQ(-DRONE_BEAM_HALF, b.base[0].y);
	CHECK_EQ(SQUARE_MIDDLE + DRONE_BEAM_HALF, b.base[2].x); // far right
	CHECK_EQ(DRONE_BEAM_HALF, b.base[2].y);
	for (int k = 0; k < 4; ++k)
		CHECK_EQ(300 - DRONE_BEAM_DROP, b.base[k].z);
}

// Inside the pyramid: seen (nothing in the way). Behind it, aside, past it, over the sensor: not.
// And outside, the sides that face you are the ones you're out past
static void sees_you_inside_the_beam(void)
{
	drone d;
	drone_beam b;

	drone_init(&d, high_west, 1, false, 1);
	drone_beam_get(&d, &b);
	CHECK_EQ(0, drone_beam_outside(&b, SQUARE_MIDDLE, 0, -150));
	CHECK(drone_sees(&d, SQUARE_MIDDLE, 0, -150, 0, 0));
	CHECK(drone_sees(&d, SQUARE_MIDDLE, 250, -150, 0, 0)); // aside, but inside
	CHECK(!drone_sees(&d, -500, 0, -150, 0, 0));		   // behind it
	CHECK(!drone_sees(&d, SQUARE_MIDDLE, 600, -150, 0, 0)); // well aside
	CHECK(!drone_sees(&d, SQUARE_MIDDLE + 500, 0, -150, 0, 0));
	CHECK(!drone_sees(&d, SQUARE_MIDDLE, 0, 400, 0, 0)); // over it
	// Out to the -y side: only side 3 (far left to near left) faces you; to +y, only side 1
	CHECK_EQ(1u << 3, drone_beam_outside(&b, SQUARE_MIDDLE, -2000, -150));
	CHECK_EQ(1u << 1, drone_beam_outside(&b, SQUARE_MIDDLE, 2000, -150));
	// Beyond the square, low: the far side faces you
	CHECK(drone_beam_outside(&b, SQUARE_MIDDLE + 800, 0, -200) & (1u << 2));
	CHECK_EQ(0xFu, drone_beam_outside(&b, 5000, 0, 0)); // out of its reach
}

/*
 * Inside is exactly the pyramid, checked against the plain sum (floats: the PC only): a point's in
 * it if it's under the sensor and the line from the sensor through it meets the base's plane
 * inside the square. Points within a unit of a side's plane are let off (rounding)
 */
static void inside_is_the_pyramid(void)
{
	drone d;
	drone_beam b;
	unsigned int seed = 99;

	drone_init(&d, high_west, 1, false, 1);
	drone_beam_get(&d, &b);
	for (int i = 0; i < 20000; ++i)
	{
		int x, y, z;

		seed = seed * 1103515245u + 12345u;
		x = (int)((seed >> 8) % 2400) - 600;
		seed = seed * 1103515245u + 12345u;
		y = (int)((seed >> 8) % 2400) - 1200;
		seed = seed * 1103515245u + 12345u;
		z = (int)((seed >> 8) % 1400) - 1000;
		{
			const bool inside = drone_beam_outside(&b, x, y, z) == 0;
			// How far down to the base the point is, from the sensor: 1 at the base
			const double s = (double)(z - b.apex.z) / (double)(b.base[0].z - b.apex.z);
			const double qx = b.apex.x + (x - b.apex.x) / s, qy = b.apex.y + (y - b.apex.y) / s;
			const double ex = (qx < b.base[0].x ? b.base[0].x - qx : qx - b.base[2].x) * s;
			const double ey = (qy < b.base[0].y ? b.base[0].y - qy : qy - b.base[2].y) * s;

			if (s <= 0.01)
				CHECK(!inside); // over the sensor, or level with it
			else if (ex > 1.0 || ey > 1.0)
				CHECK(!inside);
			else if (ex < -1.0 && ey < -1.0)
				CHECK(inside);
		}
	}
}

// A tower between the sensor and your eye hides you; a low one doesn't
static void a_tower_in_the_way_hides_you(void)
{
	drone d;
	drone_box tall, low;

	drone_init(&d, high_west, 1, false, 1);
	tall.x_min = SQUARE_MIDDLE - 300;
	tall.x_max = SQUARE_MIDDLE - 100;
	tall.y_min = -100;
	tall.y_max = 100;
	tall.top = 200; // nearly up to the sensor: the line from it down to you passes through it
	low = tall;
	low.top = -250;
	CHECK(!drone_sees(&d, SQUARE_MIDDLE, 0, -150, &tall, 1));
	CHECK(drone_sees(&d, SQUARE_MIDDLE, 0, -150, &low, 1));
}

// Its beam's square eases down (or up) to the roof under its middle
static void the_beam_settles_on_the_roof_under_it(void)
{
	drone d;
	drone_box roof;

	drone_init(&d, high_west, 1, false, 1);
	roof.x_min = SQUARE_MIDDLE - 400;
	roof.x_max = SQUARE_MIDDLE + 400;
	roof.y_min = -400;
	roof.y_max = 400;
	roof.top = -50;
	for (int i = 0; i < 400; ++i)
	{
		d.yaw = HEADING_WEST << 14; // held facing west (a glance would turn it off the roof)
		drone_update(&d, &roof, 1);
	}
	CHECK_EQ(-50, d.beam_z);
}

// Facing an axis it's settled; a few degrees off still; more and it isn't
static void settles_only_near_an_axis(void)
{
	drone d;

	drone_init(&d, high_west, 1, false, 1);
	d.yaw = (HEADING_WEST << 14) + 0x0400;
	CHECK_EQ(HEADING_WEST, drone_settled_heading(&d));
	d.yaw = (HEADING_WEST << 14) + 0x1000;
	CHECK_EQ(-1, drone_settled_heading(&d));
	d.yaw = 0xFF00; // just short of north, round the top
	CHECK_EQ(HEADING_NORTH, drone_settled_heading(&d));
}

// The scan line sweeps across the square, near edge to far, then flies back
static void the_scan_line_sweeps_the_square(void)
{
	drone d;

	drone_init(&d, high_west, 1, false, 1);
	CHECK_EQ(DRONE_BEAM_AHEAD - DRONE_BEAM_HALF, drone_scan_ahead(&d));
	d.scan = DRONE_SCAN_FRAMES - 1;
	CHECK(drone_scan_ahead(&d) + DRONE_SCAN_DEEP <= DRONE_BEAM_AHEAD + DRONE_BEAM_HALF);
	CHECK(drone_scan_ahead(&d) + DRONE_SCAN_DEEP > DRONE_BEAM_AHEAD + DRONE_BEAM_HALF - 20);
}

// The line lights the part of each roof under it, clipped to the roof; not one in a taller one's
// shadow; none while it's turned off an axis
static void the_line_lights_the_roofs_under_it(void)
{
	drone d;
	drone_box towers[3], lit[DRONE_LIT_MAX];
	int x, count;

	drone_init(&d, high_west, 1, false, 1);
	d.scan = DRONE_SCAN_FRAMES / 2;
	x = 140 + drone_scan_ahead(&d);
	// A roof under the line, wider than it
	towers[0].x_min = (short)(x - 200);
	towers[0].x_max = (short)(x + 300);
	towers[0].y_min = -250;
	towers[0].y_max = 250;
	towers[0].top = -100;
	// One off the line, beyond it
	towers[1] = towers[0];
	towers[1].x_min = (short)(x + 400);
	towers[1].x_max = (short)(x + 800);
	count = drone_scan_lit(&d, towers, 2, lit);
	CHECK_EQ(1, count);
	CHECK_EQ(x, lit[0].x_min);
	CHECK_EQ(x + DRONE_SCAN_DEEP, lit[0].x_max);
	CHECK_EQ(-250, lit[0].y_min);
	CHECK_EQ(250, lit[0].y_max);
	CHECK_EQ(-100, lit[0].top);
	// A tall tower nearer the drone puts it in shadow
	towers[2].x_min = (short)(x - 350);
	towers[2].x_max = (short)(x - 210);
	towers[2].y_min = -300;
	towers[2].y_max = 300;
	towers[2].top = 250;
	CHECK_EQ(0, drone_scan_lit(&d, towers, 3, lit));
	// Turned off the axis: nothing
	d.yaw = (HEADING_WEST << 14) + 0x1000;
	CHECK_EQ(0, drone_scan_lit(&d, towers, 1, lit));
}

// A wide flat roof under everything, so the beam has somewhere to settle: 0 high
static const drone_box ground = {-8000, 8000, -8000, 8000, 0};

// How far apart (the longer way plus half the shorter, as the drone measures)
static int reach(int dx, int dy)
{
	dx = dx < 0 ? -dx : dx;
	dy = dy < 0 ? -dy : dy;
	return dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
}

/*
 * Seen, it chases: towards you, faster than its patrol but slower than your run, and holds back
 * DRONE_STANDOFF (you in its beam's middle), high over where it saw you
 */
static void seen_it_chases_and_holds_back(void)
{
	drone d;

	drone_init(&d, square, 4, false, 5);
	drone_notice(&d, true, 3000, 0, 68);
	CHECK_EQ(DRONE_CHASE, d.mode);
	for (int i = 0; i < 1500; ++i)
	{
		const int x = d.x, y = d.y;

		drone_update(&d, &ground, 1);
		drone_notice(&d, true, 3000, 0, 68);
		CHECK((d.x - x) * (d.x - x) + (d.y - y) * (d.y - y) <= 6 * 6); // never as fast as your run
	}
	CHECK(reach(3000 - d.x, 0 - d.y) <= DRONE_STANDOFF + 10);
	CHECK(reach(3000 - d.x, 0 - d.y) >= DRONE_STANDOFF - 20);
	CHECK_EQ(68 + DRONE_CHASE_ABOVE, d.z);
}

// DRONE_CATCH_FRAMES (2.6 s) in its beam and you're caught; out of it a while, the count comes back down
static void seen_long_enough_is_caught(void)
{
	drone d;

	drone_init(&d, square, 4, false, 5);
	for (int i = 0; i < DRONE_CATCH_FRAMES - 1; ++i)
		drone_notice(&d, true, 1000, 0, 68);
	CHECK(!drone_caught(&d));
	for (int i = 0; i < 30; ++i)
		drone_notice(&d, false, 0, 0, 0);
	for (int i = 0; i < 30; ++i)
		drone_notice(&d, true, 1000, 0, 68);
	CHECK(!drone_caught(&d)); // 30 down, 30 up: still one short
	drone_notice(&d, true, 1000, 0, 68);
	CHECK(drone_caught(&d));
}

/*
 * Lost: it goes to where it last saw you, right over it, and searches there: hopping no further
 * than DRONE_SEARCH_REACH from it, for DRONE_SEARCH_FRAMES; then back to its patrol, and arrives
 * at a stop of it
 */
static void lost_it_searches_then_goes_back(void)
{
	drone d;
	int frames = 0;

	drone_init(&d, square, 4, false, 5);
	drone_notice(&d, true, 2500, 1000, 68);
	while (d.mode == DRONE_CHASE && frames++ < 3000)
	{
		drone_update(&d, &ground, 1);
		drone_notice(&d, false, 0, 0, 0);
	}
	CHECK_EQ(DRONE_SEARCH, d.mode);
	CHECK(reach(2500 - d.x, 1000 - d.y) <= DRONE_SEARCH_THERE + 10);
	frames = 0;
	while (d.mode == DRONE_SEARCH && frames++ < 1000)
	{
		drone_update(&d, &ground, 1);
		drone_notice(&d, false, 0, 0, 0);
		CHECK(d.x >= 2500 - DRONE_SEARCH_REACH - 100 && d.x <= 2500 + DRONE_SEARCH_REACH + 100);
		CHECK(d.y >= 1000 - DRONE_SEARCH_REACH - 100 && d.y <= 1000 + DRONE_SEARCH_REACH + 100);
	}
	CHECK_EQ(DRONE_PATROL, d.mode);
	CHECK(frames >= DRONE_SEARCH_FRAMES - 2 && frames <= DRONE_SEARCH_FRAMES + 2);
	CHECK(d.returning);
	frames = 0;
	while (!d.waiting && frames++ < 3000)
	{
		drone_update(&d, &ground, 1);
		drone_notice(&d, false, 0, 0, 0);
	}
	CHECK(d.waiting);
	CHECK(!d.returning);
	CHECK_EQ(square[d.at].x, d.x);
	CHECK_EQ(square[d.at].y, d.y);
}

// Searching, it sees you again: it chases again
static void searching_it_finds_you_again(void)
{
	drone d;

	drone_init(&d, square, 4, false, 5);
	d.mode = DRONE_SEARCH;
	d.search_left = 100;
	drone_notice(&d, true, 500, 500, 68);
	CHECK_EQ(DRONE_CHASE, d.mode);
}

// Off its patrol, it climbs clear of a tall tower near it, whatever height it'd chase at
static void off_patrol_it_keeps_clear_of_the_towers(void)
{
	drone d;
	drone_box towers[2];

	towers[0] = ground;
	towers[1].x_min = -100;
	towers[1].x_max = 100;
	towers[1].y_min = -100;
	towers[1].y_max = 100;
	towers[1].top = 900;
	drone_init(&d, square, 4, false, 5);
	for (int i = 0; i < 300; ++i)
	{
		drone_update(&d, towers, 2);
		drone_notice(&d, true, 1200, 0, 68); // close: it holds back, near the tower
	}
	CHECK_EQ(900 + DRONE_CLEAR, d.z);
}

void run_drone_tests(void)
{
	seen_it_chases_and_holds_back();
	seen_long_enough_is_caught();
	lost_it_searches_then_goes_back();
	searching_it_finds_you_again();
	off_patrol_it_keeps_clear_of_the_towers();
	starts_waiting_at_its_first_stop();
	faces_the_way_its_yaw_says();
	flies_to_its_next_stop_and_arrives();
	turns_on_the_spot_to_a_stop_behind_it();
	a_loop_goes_round_and_sometimes_back();
	back_and_forth_turns_back_at_its_ends();
	waits_vary();
	the_same_seed_flies_the_same_patrol();
	the_beam_is_a_square_ahead_and_below();
	sees_you_inside_the_beam();
	inside_is_the_pyramid();
	a_tower_in_the_way_hides_you();
	the_beam_settles_on_the_roof_under_it();
	settles_only_near_an_axis();
	the_scan_line_sweeps_the_square();
	the_line_lights_the_roofs_under_it();
}
