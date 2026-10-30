#include "test.h"
#include "../src/game/minimap.h"

// Remember the directions: north is -y, east is -x, south is +y, west is +x (world.h)
#define FACING_NORTH 0, -1024
#define FACING_EAST -1024, 0
// The world distance that shows as n pixels on the map: the tests hold at any zoom (MINIMAP_SHIFT)
#define PX(n) ((short)((n) << MINIMAP_SHIFT))

static world_point pt(short x, short y)
{
	world_point p;
	p.x = x;
	p.y = y;
	return p;
}

static minimap_line lines[MINIMAP_LINES_MAX];

// One trail from a to b (a straight line, no corners)
static int build_one(world_point a, world_point b, world_point eye, int fx, int fy)
{
	static trail t;
	const trail *trails[1];

	trails[0] = &t;
	trail_reset(&t, a);
	trail_set_head(&t, b);
	return minimap_build(trails, 1, eye, fx, fy, lines, MINIMAP_LINES_MAX);
}

static void half_width_is_the_circle(void)
{
	minimap_init();
	CHECK_EQ(MINIMAP_RANGE - 1, minimap_half_width(0)); // sqrt(range^2 - band^2), the first band's far edge: just under
	CHECK_EQ(0, minimap_half_width(MINIMAP_RANGE));
	CHECK_EQ(0, minimap_half_width(-MINIMAP_RANGE - 5));
	// Never wider than the real circle anywhere in a band, so lines never poke out of it
	for (int c = 0; c < MINIMAP_RANGE; c += 7)
	{
		int w = minimap_half_width(c);
		CHECK((long)w * w + (long)c * c <= (long)MINIMAP_RANGE * MINIMAP_RANGE);
		CHECK_EQ(w, minimap_half_width(-c)); // the same on both sides
	}
}

static void trail_behind_is_below_the_centre(void)
{
	// Facing north, the trail it laid runs back south: straight down from the centre
	CHECK_EQ(1, build_one(pt(0, PX(10)), pt(0, 0), pt(0, 0), FACING_NORTH));
	CHECK_EQ(0, lines[0].x0);
	CHECK_EQ(10, lines[0].y0);
	CHECK_EQ(0, lines[0].x1);
	CHECK_EQ(0, lines[0].y1);
	CHECK_EQ(0, lines[0].trail);
}

static void map_turns_with_the_player(void)
{
	// A trail 10 pixels' worth to the north, crossing in front. Facing north it's above the centre...
	CHECK_EQ(1, build_one(pt(-PX(5), -PX(10)), pt(PX(5), -PX(10)), pt(0, 0), FACING_NORTH));
	CHECK_EQ(-10, lines[0].y0);
	CHECK_EQ(-10, lines[0].y1);
	// (west, +x, is on the left facing north)
	CHECK_EQ(5, lines[0].x0);
	CHECK_EQ(-5, lines[0].x1);
	// ...facing east, north is on the left: the same trail is a vertical line left of centre
	CHECK_EQ(1, build_one(pt(-PX(5), -PX(10)), pt(PX(5), -PX(10)), pt(0, 0), FACING_EAST));
	CHECK_EQ(-10, lines[0].x0);
	CHECK_EQ(-10, lines[0].x1);
	CHECK_EQ(-5, lines[0].y0); // its east end (-x) is ahead: up
	CHECK_EQ(5, lines[0].y1);
}

static void halfway_through_a_turn_it_is_diagonal(void)
{
	// Facing north-east (cos 45 = 724 / 1024), a point 16 pixels' worth to the north is up and left
	CHECK_EQ(1, build_one(pt(0, -PX(16)), pt(0, -PX(16) - 1), pt(0, 0), -724, -724));
	CHECK_EQ(-11, lines[0].x0); // 16 * 0.707 = 11.3
	CHECK_EQ(-11, lines[0].y0);
}

static void it_centres_on_the_player(void)
{
	// The same trail, the player moved: everything is relative to the player
	CHECK_EQ(1, build_one(pt(5000, PX(10) + 300), pt(5000, 300), pt(5000, 300), FACING_NORTH));
	CHECK_EQ(10, lines[0].y0);
	CHECK_EQ(0, lines[0].y1);
}

static void long_trail_is_clipped_to_the_circle(void)
{
	int r2 = (MINIMAP_RADIUS + 1) * (MINIMAP_RADIUS + 1); // 1 pixel for rounding

	// A trail right across the arena, 15 pixels' worth to the side: only the chord inside the circle
	CHECK_EQ(1, build_one(pt(-PX(15), -8000), pt(-PX(15), 8000), pt(0, 0), FACING_NORTH));
	CHECK(lines[0].x0 * lines[0].x0 + lines[0].y0 * lines[0].y0 <= r2);
	CHECK(lines[0].x1 * lines[0].x1 + lines[0].y1 * lines[0].y1 <= r2);
	CHECK(lines[0].y0 < -20 && lines[0].y1 > 20); // but most of the way across it (it starts ahead: up)
}

static void far_trail_is_not_drawn(void)
{
	CHECK_EQ(0, build_one(pt(-5000, MINIMAP_RANGE + 10), pt(5000, MINIMAP_RANGE + 10), pt(0, 0), FACING_NORTH)); // beyond the range
	CHECK_EQ(0, build_one(pt(100, -MINIMAP_RANGE - 10), pt(100, -6000), pt(0, 0), FACING_NORTH)); // in line, but past the end
}

static void every_trail_and_corner_is_drawn(void)
{
	static trail a;
	static trail b;
	const trail *both[2];
	int n;

	// Trail a has a corner: two segments. Trail b is one: three lines, each tagged with its trail
	trail_reset(&a, pt(0, PX(10)));
	trail_set_head(&a, pt(0, 0));
	trail_add_corner(&a);
	trail_set_head(&a, pt(-PX(5), 0));
	trail_reset(&b, pt(PX(5), -PX(5)));
	trail_set_head(&b, pt(PX(5), PX(5)));
	both[0] = &a;
	both[1] = &b;
	n = minimap_build(both, 2, pt(0, 0), FACING_NORTH, lines, MINIMAP_LINES_MAX);
	CHECK_EQ(3, n);
	CHECK_EQ(0, lines[0].trail);
	CHECK_EQ(0, lines[1].trail);
	CHECK_EQ(1, lines[2].trail);
	// The cap is respected
	CHECK_EQ(2, minimap_build(both, 2, pt(0, 0), FACING_NORTH, lines, 2));
}

static void tilt_keeps_the_centre_and_the_near_edge(void)
{
	int x, y;

	minimap_init();
	// The player stays at the map's centre
	minimap_tilt(0, 0, &x, &y);
	CHECK_EQ(0, x);
	CHECK_EQ(0, y);
	// The near (bottom) edge keeps its full width
	minimap_tilt(-MINIMAP_EDGE, MINIMAP_EDGE, &x, &y);
	CHECK_EQ(-MINIMAP_EDGE, x);
}

static void far_half_is_smaller_in_perspective(void)
{
	int top_x, top_y, bottom_x, bottom_y;

	minimap_init();
	minimap_tilt(MINIMAP_EDGE, -MINIMAP_EDGE, &top_x, &top_y);
	minimap_tilt(MINIMAP_EDGE, MINIMAP_EDGE, &bottom_x, &bottom_y);
	CHECK(top_x < bottom_x);	 // the far edge is narrower...
	CHECK(-top_y < bottom_y);	 // ...and the far half shorter than the near half
	CHECK(-top_y > bottom_y / 2); // but not squashed flat
}

// A real projection keeps straight lines straight (the old linear stretch didn't: the map's
// circle came out bent). Points along a diagonal map line must land on the line between its ends
static void straight_lines_stay_straight(void)
{
	int ax, ay, bx, by, px, py;

	minimap_init();
	minimap_tilt(-30, -30, &ax, &ay);
	minimap_tilt(30, 30, &bx, &by);
	for (int k = -25; k <= 25; k += 5)
	{
		minimap_tilt(k, k, &px, &py);
		// Distance from the line, x its length (the cross product): within 1.5 pixels of rounding
		long cross = (long)(bx - ax) * (py - ay) - (long)(by - ay) * (px - ax);
		long length2 = (long)(bx - ax) * (bx - ax) + (long)(by - ay) * (by - ay);
		CHECK(4 * cross * cross <= 9 * length2);
	}
}

static void tilt_narrows_steadily_towards_the_top(void)
{
	int x, y, last_x = 1000, last_y = 1000;

	minimap_init();
	for (int row = MINIMAP_EDGE; row >= -MINIMAP_EDGE; --row) // bottom to top
	{
		minimap_tilt(MINIMAP_EDGE, row, &x, &y);
		CHECK(x <= last_x); // never wider further up
		CHECK(y <= last_y); // never lower (67 map rows share ~35 screen rows, so some pairs are level)
		last_x = x;
		last_y = y;
		// Mirror images either side of the centre line
		minimap_tilt(-MINIMAP_EDGE, row, &x, &y);
		CHECK_EQ(-last_x, x);
	}
}

static void gauge_points_go_up_the_right_side(void)
{
	int x, y;

	minimap_gauge_point(0, 30, &x, &y); // the bottom
	CHECK_EQ(0, x);
	CHECK_EQ(30, y);
	minimap_gauge_point(MINIMAP_GAUGE_STEPS / 2, 30, &x, &y); // halfway: straight out to the right
	CHECK_EQ(30, x);
	CHECK_EQ(0, y);
	minimap_gauge_point(MINIMAP_GAUGE_STEPS, 30, &x, &y); // the top
	CHECK_EQ(0, x);
	CHECK_EQ(-30, y);
	// Every point is on the circle (within rounding)
	for (int k = 0; k <= MINIMAP_GAUGE_STEPS; ++k)
	{
		minimap_gauge_point(k, MINIMAP_GAUGE_OUTER, &x, &y);
		CHECK(x >= 0);
		CHECK(x * x + y * y >= (MINIMAP_GAUGE_OUTER - 1) * (MINIMAP_GAUGE_OUTER - 1));
		CHECK(x * x + y * y <= (MINIMAP_GAUGE_OUTER + 1) * (MINIMAP_GAUGE_OUTER + 1));
	}
}

static void gauge_lights_in_proportion(void)
{
	const int full = MINIMAP_GAUGE_STEPS * 16;

	CHECK_EQ(0, MINIMAP_GAUGE_LIT(0, 180));
	CHECK_EQ(full, MINIMAP_GAUGE_LIT(180, 180)); // full lights it all
	CHECK_EQ(full / 2, MINIMAP_GAUGE_LIT(90, 180));
	for (int level = 1; level <= 180; ++level)
		CHECK(MINIMAP_GAUGE_LIT(level, 180) >= MINIMAP_GAUGE_LIT(level - 1, 180)); // never goes backwards
}

// Directions for the rim markers: facing north (-y); east is -x, on the right
static void direction_on_the_rim(void)
{
	int side, k;

	minimap_direction(0, -5000, FACING_NORTH, &side, &k); // straight ahead: the top
	CHECK_EQ(MINIMAP_GAUGE_STEPS, k);
	minimap_direction(0, 5000, FACING_NORTH, &side, &k); // straight behind: the bottom
	CHECK_EQ(0, k);
	minimap_direction(-5000, 0, FACING_NORTH, &side, &k); // east: right, level
	CHECK_EQ(0, side);
	CHECK_EQ(6, k);
	minimap_direction(5000, 0, FACING_NORTH, &side, &k); // west: left, level
	CHECK_EQ(1, side);
	CHECK_EQ(6, k);
	minimap_direction(-3000, -3000, FACING_NORTH, &side, &k); // ahead and right, 45 degrees
	CHECK_EQ(0, side);
	CHECK_EQ(9, k);
	minimap_direction(-1000, -3730, FACING_NORTH, &side, &k); // 15 degrees right of ahead
	CHECK_EQ(0, side);
	CHECK_EQ(11, k);
	// Facing east, something to the north is on the left, level
	minimap_direction(0, -5000, FACING_EAST, &side, &k);
	CHECK_EQ(1, side);
	CHECK_EQ(6, k);
	// Close or far, the same direction (the far side of the arena too: no overflow)
	minimap_direction(-30, -30, FACING_NORTH, &side, &k);
	CHECK_EQ(9, k);
	minimap_direction(-28000, -28000, FACING_NORTH, &side, &k);
	CHECK_EQ(9, k);
}

void run_minimap_tests(void)
{
	RUN(direction_on_the_rim);
	RUN(gauge_points_go_up_the_right_side);
	RUN(gauge_lights_in_proportion);
	RUN(tilt_keeps_the_centre_and_the_near_edge);
	RUN(far_half_is_smaller_in_perspective);
	RUN(straight_lines_stay_straight);
	RUN(tilt_narrows_steadily_towards_the_top);
	RUN(half_width_is_the_circle);
	RUN(trail_behind_is_below_the_centre);
	RUN(map_turns_with_the_player);
	RUN(halfway_through_a_turn_it_is_diagonal);
	RUN(it_centres_on_the_player);
	RUN(long_trail_is_clipped_to_the_circle);
	RUN(far_trail_is_not_drawn);
	RUN(every_trail_and_corner_is_drawn);
}
