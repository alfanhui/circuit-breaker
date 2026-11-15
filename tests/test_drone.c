#include "test.h"
#include "../src/game/drone.h"
#include "../src/game/world.h"

// Two stops 120 apart in height (a floor down, a face of a tower) and 60 along: waits 10, then 5
static const drone_stop two_floors[2] = {
	{0, 0, 0, HEADING_WEST, 10},
	{0, 60, -120, HEADING_SOUTH, 5},
};

static void starts_waiting_at_its_first_stop(void)
{
	drone d;

	drone_init(&d, two_floors, 2);
	CHECK(drone_waiting(&d));
	CHECK_EQ(0, d.z);
	CHECK_EQ(HEADING_WEST, drone_heading(&d));
	CHECK_EQ(HEADING_WEST << 14, d.yaw);
}

// It waits its frames, then flies at DRONE_SPEED an axis, each axis stopping where the stop is
static void waits_then_flies_to_the_next(void)
{
	drone d;

	drone_init(&d, two_floors, 2);
	for (int i = 0; i < 10; ++i)
	{
		CHECK(drone_waiting(&d));
		drone_update(&d);
	}
	CHECK_EQ(1, d.at); // set off for the second
	CHECK(!drone_waiting(&d));
	drone_update(&d);
	CHECK_EQ(DRONE_SPEED, d.y);
	CHECK_EQ(-DRONE_SPEED, d.z);
	// 60 along is done in 10 frames, the 120 down in 20: then it's there, waiting
	for (int i = 1; i < 20; ++i)
		drone_update(&d);
	CHECK_EQ(60, d.y);
	CHECK_EQ(-120, d.z);
	CHECK(drone_waiting(&d));
}

// After the last stop, round to the first
static void goes_round_its_route(void)
{
	drone d;

	drone_init(&d, two_floors, 2);
	for (int i = 0; i < 10 + 20 + 5; ++i)
		drone_update(&d);
	CHECK_EQ(0, d.at);
	for (int i = 0; i < 20; ++i)
		drone_update(&d);
	CHECK(drone_waiting(&d));
	CHECK_EQ(0, d.z);
}

// It turns to the stop's heading the short way, easing, and gets there exactly
static void turns_the_short_way_and_arrives(void)
{
	static const drone_stop turn[2] = {{0, 0, 0, HEADING_NORTH, 1}, {0, 0, 0, HEADING_WEST, 1000}};
	drone d;
	int previous;

	drone_init(&d, turn, 2);
	drone_update(&d); // waited: now at stop 1 (the same place), facing west to come
	drone_update(&d);
	// North (0) to west (0xC000): the short way is back through 0xFFFF, a quarter turn
	CHECK(d.yaw > 0xC000);
	previous = d.yaw;
	for (int i = 0; i < 200; ++i)
	{
		drone_update(&d);
		CHECK(d.yaw <= previous || d.yaw == 0xC000); // never back the other way
		previous = d.yaw;
	}
	CHECK_EQ(0xC000, d.yaw);
}

// The drone up at (0, 0, 300), facing west (+x): its sensor 140 ahead of it, at x 140
static const drone_stop high_west[1] = {{0, 0, 300, HEADING_WEST, 60000}};

// The scan line sweeps out from near to its reach, a step a frame, then flies back
static void the_scan_line_sweeps_out_then_back(void)
{
	drone d;
	int previous;

	drone_init(&d, high_west, 1);
	CHECK_EQ(DRONE_SCAN_NEAR, drone_scan_ahead(&d));
	previous = drone_scan_ahead(&d);
	for (int i = 1; i < DRONE_SCAN_FRAMES; ++i)
	{
		drone_update(&d);
		CHECK(drone_scan_ahead(&d) > previous);
		previous = drone_scan_ahead(&d);
	}
	CHECK(previous <= DRONE_SCAN_RANGE);
	CHECK(previous > DRONE_SCAN_RANGE - 20);
	drone_update(&d);
	CHECK_EQ(DRONE_SCAN_NEAR, drone_scan_ahead(&d));
}

// The drone facing west, its line `ahead` out from the sensor
static void line_at(drone *d, int ahead)
{
	drone_init(d, high_west, 1);
	while (drone_scan_ahead(d) < ahead)
		drone_update(d);
}

// The line's a rectangle: from its near edge to DRONE_SCAN_DEEP further, as wide as the fan there
static void the_line_is_as_wide_as_the_fan(void)
{
	drone d;
	drone_box r;

	line_at(&d, 600);
	drone_scan_rect(&d, &r);
	CHECK_EQ(140 + drone_scan_ahead(&d), r.x_min);
	CHECK_EQ(r.x_min + DRONE_SCAN_DEEP, r.x_max);
	CHECK_EQ(-(drone_scan_ahead(&d) + DRONE_SCAN_DEEP), r.y_min);
	CHECK_EQ(-r.y_min, r.y_max);
}

// Under the line, in the open: seen. Off it (before it, after it, aside of the fan): not
static void sees_you_when_the_line_is_under_you(void)
{
	drone d;
	int x;

	line_at(&d, 600);
	x = 140 + drone_scan_ahead(&d) + 20; // the sensor's x, the line's near edge, into it
	CHECK(drone_sees(&d, x, 0, 68, 0, 0));
	CHECK(drone_sees(&d, x, 600, 68, 0, 0)); // well aside, but within the fan
	CHECK(!drone_sees(&d, x - 100, 0, 68, 0, 0));
	CHECK(!drone_sees(&d, x + 100, 0, 68, 0, 0));
	CHECK(!drone_sees(&d, x, 800, 68, 0, 0));
	CHECK(!drone_sees(&d, -x, 0, 68, 0, 0)); // behind it
}

// A tower between the sensor and your eye hides you; a low one doesn't
static void a_tower_in_the_way_hides_you(void)
{
	drone d;
	int x;
	drone_box tall, low;

	line_at(&d, 800);
	x = 140 + drone_scan_ahead(&d) + 20;
	tall.x_min = (short)(x - 300);
	tall.x_max = (short)(x - 100);
	tall.y_min = -100;
	tall.y_max = 100;
	tall.top = 280; // nearly up to the drone: the line from the sensor down to you passes through it
	low = tall;
	low.top = 0;
	CHECK(!drone_sees(&d, x, 0, 68, &tall, 1));
	CHECK(drone_sees(&d, x, 0, 68, &low, 1));
}

// The line lights the part of each roof under it, clipped to the roof; not a roof in a taller
// one's shadow, nor one off the line
static void the_line_lights_the_roofs_under_it(void)
{
	drone d;
	drone_box towers[3], lit[DRONE_LIT_MAX];
	int x, count;

	line_at(&d, 800);
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
	count = drone_scan_lit(&d, towers, 3, lit);
	CHECK_EQ(0, count);
	CHECK_EQ(-100, drone_scan_ground(&d, towers, 1, -999));
	CHECK_EQ(-999, drone_scan_ground(&d, &towers[1], 1, -999)); // a street there
}

void run_drone_tests(void)
{
	starts_waiting_at_its_first_stop();
	waits_then_flies_to_the_next();
	goes_round_its_route();
	turns_the_short_way_and_arrives();
	the_scan_line_sweeps_out_then_back();
	the_line_is_as_wide_as_the_fan();
	sees_you_when_the_line_is_under_you();
	a_tower_in_the_way_hides_you();
	the_line_lights_the_roofs_under_it();
}
