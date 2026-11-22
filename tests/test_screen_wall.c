#include "test.h"
#include "../src/game/screen_wall.h"

/*
 * SG-9 (game/screen_wall.h): a wall that covers the whole screen, and how deep it is. A camera at
 * the origin, 35 up, looking along +x (on foot: the screen's edges at tan 37.5 degrees aside, and
 * 0.75 of that up). Walls are upright rectangles in the plane x = line
 */
static void camera(frustum *f, int forward_x, int forward_y)
{
	const frustum_point eye = {0, 0, 35};
	const int forward[3] = {forward_x, forward_y, 0};
	const int right[3] = {forward_y, -forward_x, 0}; // a quarter turn on, along the floor
	const int up[3] = {0, 0, 1024};

	frustum_set_camera(f, eye, forward, right, up, 786, 590, 8);
}

// Square on, close, wide and tall: it covers, and every corner's 100 deep
static void a_wall_in_front_covers(void)
{
	frustum f;

	camera(&f, 1024, 0);
	CHECK_EQ(100, screen_wall_depth(&f, false, 100, -500, 500, -3000, 400));
}

// Too narrow, too low, or behind you: it doesn't
static void not_wide_tall_or_in_front(void)
{
	frustum f;

	camera(&f, 1024, 0);
	CHECK_EQ(-1, screen_wall_depth(&f, false, 100, -50, 50, -3000, 400)); // the screen's sides see round it
	CHECK_EQ(-1, screen_wall_depth(&f, false, 100, -500, 500, -3000, 60)); // its top's in view
	CHECK_EQ(-1, screen_wall_depth(&f, false, -100, -500, 500, -3000, 400)); // behind
	CHECK_EQ(-1, screen_wall_depth(&f, true, 100, -500, 500, -3000, 400)); // along the view, beside you
	CHECK_EQ(-1, screen_wall_depth(&f, false, 0, -500, 500, -3000, 400));	 // edge on, through the eye
}

// Too near: the near plane cuts it, so it isn't drawn there and hides nothing (wall_turn.steps'
// first run: the eye 1 from a wall, and only the sky left on the screen). Stood against one, 40
// off (WALKER_RADIUS), it's taken
static void too_near_to_be_drawn_hides_nothing(void)
{
	frustum f;

	camera(&f, 1024, 0);
	CHECK_EQ(-1, screen_wall_depth(&f, false, 1, -500, 500, -3000, 400));
	CHECK_EQ(-1, screen_wall_depth(&f, false, SCREEN_WALL_NEAREST - 1, -500, 500, -3000, 400));
	CHECK_EQ(40, screen_wall_depth(&f, false, 40, -500, 500, -3000, 400));
}

// Further off, the same wall stops covering: the screen's corners see past its ends
static void further_off_it_stops(void)
{
	frustum f;

	camera(&f, 1024, 0);
	CHECK(screen_wall_depth(&f, false, 300, -500, 500, -3000, 400) > 0);
	CHECK_EQ(-1, screen_wall_depth(&f, false, 800, -500, 500, -3000, 400));
}

// Turned to it at an angle (30 degrees): it still covers, and its far corner is deeper than the
// wall's nearest point; never less deep than that
static void at_an_angle_the_far_corner_counts(void)
{
	frustum f;
	int depth;

	camera(&f, 887, 512);
	depth = screen_wall_depth(&f, false, 100, -800, 800, -3000, 400);
	CHECK(depth > 100);
	CHECK(depth < 400);
}

void run_screen_wall_tests(void)
{
	RUN(a_wall_in_front_covers);
	RUN(not_wide_tall_or_in_front);
	RUN(too_near_to_be_drawn_hides_nothing);
	RUN(further_off_it_stops);
	RUN(at_an_angle_the_far_corner_counts);
}
