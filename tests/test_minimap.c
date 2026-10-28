#include "test.h"
#include "../src/game/minimap.h"

// Remember the directions: north is -y, east is -x, south is +y, west is +x (world.h)
#define FACING_NORTH 0, -1024
#define FACING_EAST -1024, 0

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
	CHECK_EQ(2047, minimap_half_width(0)); // sqrt(2048^2 - 32^2): the first band's far edge
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
	CHECK_EQ(1, build_one(pt(0, 640), pt(0, 0), pt(0, 0), FACING_NORTH));
	CHECK_EQ(0, lines[0].x0);
	CHECK_EQ(10, lines[0].y0); // 640 / 64
	CHECK_EQ(0, lines[0].x1);
	CHECK_EQ(0, lines[0].y1);
	CHECK_EQ(0, lines[0].trail);
}

static void map_turns_with_the_player(void)
{
	// A trail 640 to the north, crossing in front. Facing north it's above the centre...
	CHECK_EQ(1, build_one(pt(-320, -640), pt(320, -640), pt(0, 0), FACING_NORTH));
	CHECK_EQ(-10, lines[0].y0);
	CHECK_EQ(-10, lines[0].y1);
	// (west, +x, is on the left facing north)
	CHECK_EQ(5, lines[0].x0);
	CHECK_EQ(-5, lines[0].x1);
	// ...facing east, north is on the left: the same trail is a vertical line left of centre
	CHECK_EQ(1, build_one(pt(-320, -640), pt(320, -640), pt(0, 0), FACING_EAST));
	CHECK_EQ(-10, lines[0].x0);
	CHECK_EQ(-10, lines[0].x1);
	CHECK_EQ(-5, lines[0].y0); // its east end (-x) is ahead: up
	CHECK_EQ(5, lines[0].y1);
}

static void halfway_through_a_turn_it_is_diagonal(void)
{
	// Facing north-east (cos 45 = 724 / 1024), a point 1024 to the north is up and to the left
	CHECK_EQ(1, build_one(pt(0, -1024), pt(0, -1025), pt(0, 0), -724, -724));
	CHECK_EQ(-11, lines[0].x0); // 1024 * 0.707 / 64 = 11.3
	CHECK_EQ(-11, lines[0].y0);
}

static void it_centres_on_the_player(void)
{
	// The same trail, the player moved: everything is relative to the player
	CHECK_EQ(1, build_one(pt(5000, 640 + 300), pt(5000, 300), pt(5000, 300), FACING_NORTH));
	CHECK_EQ(10, lines[0].y0);
	CHECK_EQ(0, lines[0].y1);
}

static void long_trail_is_clipped_to_the_circle(void)
{
	int r2 = (MINIMAP_RADIUS + 1) * (MINIMAP_RADIUS + 1); // 1 pixel for rounding

	// A trail right across the arena, 1000 to the side: only the chord inside the circle
	CHECK_EQ(1, build_one(pt(-1000, -8000), pt(-1000, 8000), pt(0, 0), FACING_NORTH));
	CHECK(lines[0].x0 * lines[0].x0 + lines[0].y0 * lines[0].y0 <= r2);
	CHECK(lines[0].x1 * lines[0].x1 + lines[0].y1 * lines[0].y1 <= r2);
	CHECK(lines[0].y0 < -20 && lines[0].y1 > 20); // but most of the way across it (it starts ahead: up)
}

static void far_trail_is_not_drawn(void)
{
	CHECK_EQ(0, build_one(pt(-5000, 3000), pt(5000, 3000), pt(0, 0), FACING_NORTH)); // beyond the range
	CHECK_EQ(0, build_one(pt(100, -3000), pt(100, -6000), pt(0, 0), FACING_NORTH)); // in line, but past the end
}

static void every_trail_and_corner_is_drawn(void)
{
	static trail a;
	static trail b;
	const trail *both[2];
	int n;

	// Trail a has a corner: two segments. Trail b is one: three lines, each tagged with its trail
	trail_reset(&a, pt(0, 640));
	trail_set_head(&a, pt(0, 0));
	trail_add_corner(&a);
	trail_set_head(&a, pt(-320, 0));
	trail_reset(&b, pt(300, -300));
	trail_set_head(&b, pt(300, 300));
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

void run_minimap_tests(void)
{
	RUN(half_width_is_the_circle);
	RUN(trail_behind_is_below_the_centre);
	RUN(map_turns_with_the_player);
	RUN(halfway_through_a_turn_it_is_diagonal);
	RUN(it_centres_on_the_player);
	RUN(long_trail_is_clipped_to_the_circle);
	RUN(far_trail_is_not_drawn);
	RUN(every_trail_and_corner_is_drawn);
}
