#include "test.h"
#include "../src/game/surround.h"

static bool in_rect(const world_rect *r, int x, int y)
{
	return x >= r->x_min && x <= r->x_max && y >= r->y_min && y <= r->y_max;
}

// Every spot outside the walls, out to the extent, is under exactly one piece; inside, none.
// So nothing's drawn twice and no floor shows between them
static void the_pieces_tile_the_ring(void)
{
	const int half = 8000;
	surround_piece pieces[SURROUND_PIECES];
	int wrong = 0;

	surround_pieces(half, pieces);
	// Odd steps, so the samples land inside pieces, never on their shared edges
	for (int x = -29999; x < 30000; x += 377)
		for (int y = -29999; y < 30000; y += 377)
		{
			int under = 0;
			bool outside = x < -half || x > half || y < -half || y > half;

			for (int p = 0; p < SURROUND_PIECES; ++p)
				under += in_rect(&pieces[p].area, x, y);
			if (under != (outside ? 1 : 0))
				++wrong;
		}
	CHECK_EQ(0, wrong);
}

static void the_rim_runs_along_the_walls(void)
{
	surround_piece pieces[SURROUND_PIECES];
	int rims = 0;

	surround_pieces(8000, pieces);
	for (int p = 0; p < SURROUND_PIECES; ++p)
	{
		const world_rect *r = &pieces[p].area;

		if (!pieces[p].rim)
			continue;
		++rims;
		// SURROUND_RIM across, along its wall
		if (pieces[p].side == SURROUND_NORTH || pieces[p].side == SURROUND_SOUTH)
			CHECK_EQ(SURROUND_RIM, r->y_max - r->y_min);
		else
			CHECK_EQ(SURROUND_RIM, r->x_max - r->x_min);
	}
	CHECK_EQ(4, rims);
	// The south rim starts at the south wall (+y)
	CHECK_EQ(8000, pieces[2].area.y_min);
}

static world_point pt(int x, int y)
{
	world_point p = {(short)x, (short)y};
	return p;
}

static void a_rect_inside_the_quad_is_kept_whole(void)
{
	world_rect r = {-100, -200, 300, 400};
	world_point quad[4] = {pt(-1000, -1000), pt(1000, -1000), pt(1000, 1000), pt(-1000, 1000)};
	world_point out[SURROUND_CLIP_MAX];

	CHECK_EQ(4, surround_clip(&r, quad, out));
	for (int i = 0; i < 4; ++i)
	{
		CHECK(out[i].x == r.x_min || out[i].x == r.x_max);
		CHECK(out[i].y == r.y_min || out[i].y == r.y_max);
	}
}

static void a_rect_off_the_quad_is_dropped(void)
{
	world_rect r = {5000, 5000, 6000, 6000};
	world_point quad[4] = {pt(-1000, -1000), pt(1000, -1000), pt(1000, 1000), pt(-1000, 1000)};
	world_point out[SURROUND_CLIP_MAX];

	CHECK_EQ(0, surround_clip(&r, quad, out));
	// In the quad's box, but past its slanted edge: a diamond's corner
	{
		world_point diamond[4] = {pt(0, -1000), pt(1000, 0), pt(0, 1000), pt(-1000, 0)};
		world_rect corner = {700, 700, 900, 900};

		CHECK_EQ(0, surround_clip(&corner, diamond, out));
	}
}

// Twice the signed area of a polygon (the shoelace formula), in units of 100 to fit 32 bits
static int area2(const world_point *p, int n)
{
	int sum = 0;

	for (int i = 0; i < n; ++i)
	{
		const world_point a = p[i];
		const world_point b = p[i + 1 < n ? i + 1 : 0];
		sum += (a.x / 10) * (b.y / 10) - (b.x / 10) * (a.y / 10);
	}
	return sum < 0 ? -sum : sum;
}

// A screen-shaped quad (a trapezoid, as the camera sees the floor: narrow near, wide far, turned)
// across a big rectangle: what's left is inside both, its corners in order, and the right size
static void a_wide_view_cuts_a_big_rect(void)
{
	world_point quad[4] = {pt(-3000, 4000), pt(3500, 3600), pt(14000, -20000), pt(-12000, -21000)};
	world_rect r = {-30000, -30000, 30000, -8080}; // the north body, past the rim
	world_point out[SURROUND_CLIP_MAX];
	int n = surround_clip(&r, quad, out);

	CHECK(n >= 3 && n <= SURROUND_CLIP_MAX);
	for (int i = 0; i < n; ++i)
	{
		CHECK(in_rect(&r, out[i].x, out[i].y));
		CHECK(out[i].y <= -8080);
	}
	// The quad's part past y -8080, worked out by hand: a trapezoid from y -8080 to the far edge.
	// Its left edge at y -8080: x = -3000 + (-12000 + 3000) x 12080 / 25000 = -7349; its right
	// edge: 3500 + (14000 - 3500) x 11680 / 23600 = 8697. The far edge runs from (-12000, -21000)
	// to (14000, -20000). Area: ~ (16046 + 26000) / 2 x ~12420 = ~261 million
	{
		int got = area2(out, n);			  // x 2, / 100
		int want = 2 * 261000000 / 100;		  // 5.22 million
		CHECK(got > want - want / 50 && got < want + want / 50); // within 2%
	}
}

// The corners come out on the rectangle's edges exactly where it's cut: the rim and the body
// beside it share their edge, so there's no gap between them on the screen
static void cuts_land_exactly_on_the_edge(void)
{
	world_point quad[4] = {pt(-5000, -5000), pt(5000, -5000), pt(5000, -15000), pt(-5000, -15000)};
	world_rect rim = {-8080, -8080, 8080, -8000};
	world_point out[SURROUND_CLIP_MAX];
	int n = surround_clip(&rim, quad, out);

	CHECK_EQ(4, n);
	for (int i = 0; i < n; ++i)
		CHECK(out[i].y == -8080 || out[i].y == -8000);
}

static void in_front_only_from_outside(void)
{
	CHECK(!surround_in_front(SURROUND_SOUTH, pt(0, 7000), 8000)); // inside: behind everything
	CHECK(surround_in_front(SURROUND_SOUTH, pt(0, 9000), 8000));  // out past the south wall
	CHECK(!surround_in_front(SURROUND_NORTH, pt(0, 9000), 8000));
	CHECK(surround_in_front(SURROUND_NORTH, pt(0, -9000), 8000));
	CHECK(surround_in_front(SURROUND_EAST, pt(-9000, 0), 8000)); // east is -x
	CHECK(surround_in_front(SURROUND_WEST, pt(9000, 0), 8000));
	CHECK(!surround_in_front(SURROUND_WEST, pt(-9000, 0), 8000));
}

void run_surround_tests(void)
{
	the_pieces_tile_the_ring();
	the_rim_runs_along_the_walls();
	a_rect_inside_the_quad_is_kept_whole();
	a_rect_off_the_quad_is_dropped();
	a_wide_view_cuts_a_big_rect();
	cuts_land_exactly_on_the_edge();
	in_front_only_from_outside();
}
