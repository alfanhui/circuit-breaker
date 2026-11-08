#include "test.h"
#include "../src/game/occluder.h"

static frustum_point at(int x, int y, int z)
{
	frustum_point p = {x, y, z};

	return p;
}

// A wall 400 across (x -200 .. 200) and 300 tall on the line y = -500, the eye at the origin, 50 up
static void wall_ahead(occluder *o)
{
	occluder_set(o, at(0, 0, 50), true, -500, -200, 200, 0, 300);
}

static void a_wall_hides_what_is_wholly_behind_it(void)
{
	occluder o;

	wall_ahead(&o);
	CHECK(o.on);
	// Straight behind it, small: hidden
	CHECK(occluder_hides(&o, at(0, -1000, 100), at(20, 20, 20)));
	// Twice as far, the shadow's twice the size (800 across, z -50 .. 550): a box 700 across, 460 tall fits
	CHECK(occluder_hides(&o, at(0, -1000, 300), at(350, 5, 230)));
	// Wider than the shadow there (it's 800 across at y -1000): some of it shows
	CHECK(!occluder_hides(&o, at(0, -1000, 100), at(500, 20, 20)));
	// Above it: the shadow's top at y -1000 is 50 + 250 x 2 = 550
	CHECK(!occluder_hides(&o, at(0, -1000, 600), at(20, 20, 20)));
	// In front of the wall, inside the pyramid: nothing hides it
	CHECK(!occluder_hides(&o, at(0, -300, 100), at(20, 20, 20)));
	// Crossing the wall's line: the near part is in front of it
	CHECK(!occluder_hides(&o, at(0, -500, 100), at(20, 100, 20)));
}

static void a_hole_shows_only_what_is_in_line_with_it(void)
{
	occluder o;

	wall_ahead(&o); // now as a hole in a wall along y = -500
	CHECK(occluder_shows(&o, at(0, -1000, 100), at(20, 20, 20)));
	// Partly in line with it: shows
	CHECK(occluder_shows(&o, at(500, -1000, 100), at(200, 20, 20)));
	// Wholly aside (the pyramid's 800 across at y -1000) or above it: can't be seen through it
	CHECK(!occluder_shows(&o, at(1000, -1000, 100), at(100, 20, 20)));
	CHECK(!occluder_shows(&o, at(0, -1000, 900), at(20, 20, 20)));
}

// From the other side of the line the shadow's the other way: the planes follow the eye
static void the_shadow_falls_away_from_the_eye(void)
{
	occluder o;

	occluder_set(&o, at(0, -1000, 50), true, -500, 200, -200, 0, 300); // listed the other way round too
	CHECK(o.on);
	CHECK(occluder_hides(&o, at(0, 0, 100), at(20, 20, 20)));
	CHECK(!occluder_hides(&o, at(0, -1500, 100), at(20, 20, 20))); // behind the eye
}

// A wall along y (x = line): the corridor's sides from the city
static void a_wall_along_y(void)
{
	occluder o;

	occluder_set(&o, at(1000, 0, 68), false, 500, -300, 300, -8, 498);
	CHECK(o.on);
	CHECK(occluder_hides(&o, at(0, 0, 100), at(20, 20, 20)));
	CHECK(!occluder_hides(&o, at(0, 2000, 100), at(20, 20, 20))); // far aside
	CHECK(!occluder_hides(&o, at(0, 0, 1400), at(20, 20, 20)));	   // above (the shadow's top there: ~930)
}

// Too near its line, it's off: it hides nothing, and as a hole shows everything
static void too_near_it_is_off(void)
{
	occluder o;

	occluder_set(&o, at(0, -500 + OCCLUDER_NEAREST - 1, 50), true, -500, -200, 200, 0, 300);
	CHECK(!o.on);
	CHECK(!occluder_hides(&o, at(0, -1000, 100), at(20, 20, 20)));
	CHECK(occluder_shows(&o, at(5000, -1000, 100), at(20, 20, 20)));
	occluder_set(&o, at(0, -500 + OCCLUDER_NEAREST, 50), true, -500, -200, 200, 0, 300);
	CHECK(o.on);
}

// The arena's far wall, ~20000 off, tested against the hole near it: within 32000, the sums hold
static void far_boxes_are_tested_right(void)
{
	occluder o;

	occluder_set(&o, at(12749, 114, 68), false, 8000, -500, 500, -8, 1500);
	CHECK(o.on);
	CHECK(occluder_shows(&o, at(-8000, 0, 700), at(125, 125, 700)));
	CHECK(!occluder_shows(&o, at(-8000, 6000, 700), at(125, 125, 700)));
}

void run_occluder_tests(void)
{
	RUN(a_wall_hides_what_is_wholly_behind_it);
	RUN(a_hole_shows_only_what_is_in_line_with_it);
	RUN(the_shadow_falls_away_from_the_eye);
	RUN(a_wall_along_y);
	RUN(too_near_it_is_off);
	RUN(far_boxes_are_tested_right);
}
