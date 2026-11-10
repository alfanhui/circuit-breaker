#include "test.h"
#include "../src/game/frustum.h"

// A camera at the origin, 35 up, looking north (-y) and level: right is east (-x), as view.c's
static const frustum_point EYE = {0, 0, 35};
static const int NORTH[3] = {0, -FRUSTUM_ONE, 0};
static const int EAST[3] = {-FRUSTUM_ONE, 0, 0};
static const int UP[3] = {0, 0, FRUSTUM_ONE};

// 90 degrees across (tan 45 = 1), and 0.75 of that up and down, 10 in front at the nearest
static void level_camera(frustum *f)
{
	frustum_set_camera(f, EYE, NORTH, EAST, UP, FRUSTUM_ONE, 768, 10);
}

static frustum_point at(int x, int y, int z)
{
	frustum_point p = {x, y, z};

	return p;
}

// Whether every corner's inside every plane in the mask, to within a unit (the cut's rounding)
static bool all_inside(const frustum *f, unsigned planes, const frustum_point *p, int n)
{
	for (int i = 0; i < f->count; ++i)
		if (planes & (1u << i))
			for (int k = 0; k < n; ++k)
				if (frustum_distance(f, i, p[k]) < -FRUSTUM_ONE)
					return false;
	return true;
}

static void square_roots_round_down(void)
{
	CHECK_EQ(0, frustum_isqrt(0));
	CHECK_EQ(1, frustum_isqrt(3));
	CHECK_EQ(2, frustum_isqrt(4));
	CHECK_EQ(1024, frustum_isqrt(1024 * 1024));
	CHECK_EQ(1023, frustum_isqrt(1024 * 1024 - 1));
	CHECK_EQ(28376, frustum_isqrt(3u * 16383 * 16383)); // 16383 x 1.732: 28376.07
}

// The planes are 1024 long: a point's distance comes out in world units x 1024
static void distances_are_in_world_units(void)
{
	frustum f;

	level_camera(&f);
	CHECK_EQ(FRUSTUM_PORTAL_FIRST, f.count);
	// 100 in front: 90 past the near plane
	CHECK_EQ(90 * FRUSTUM_ONE, frustum_distance(&f, FRUSTUM_NEAR, at(0, -100, 35)));
	CHECK(frustum_distance(&f, FRUSTUM_NEAR, at(0, 50, 35)) < 0); // behind
}

static void what_the_camera_sees(void)
{
	frustum f;
	const frustum_point ahead[1] = {{0, -500, 35}};
	const frustum_point behind[1] = {{0, 500, 35}};
	const frustum_point aside[1] = {{-600, -500, 35}};	// east of the 45 degree edge
	const frustum_point above[1] = {{0, -500, 35 + 400}}; // over the top edge (0.75 x 500 = 375)
	const frustum_point below[1] = {{0, -500, 35 - 300}}; // inside the bottom edge
	// A box well in front, all of it on the screen
	const frustum_point box[8] = {{-50, -400, 0}, {50, -400, 0},	{50, -500, 0},	{-50, -500, 0},
								  {-50, -400, 60}, {50, -400, 60}, {50, -500, 60}, {-50, -500, 60}};

	level_camera(&f);
	CHECK_EQ(0, frustum_classify(&f, ahead, 1, FRUSTUM_SCREEN));
	CHECK_EQ(FRUSTUM_OUTSIDE, frustum_classify(&f, behind, 1, FRUSTUM_SCREEN));
	CHECK_EQ(FRUSTUM_OUTSIDE, frustum_classify(&f, aside, 1, FRUSTUM_SCREEN));
	CHECK_EQ(FRUSTUM_OUTSIDE, frustum_classify(&f, above, 1, FRUSTUM_SCREEN));
	CHECK_EQ(0, frustum_classify(&f, below, 1, FRUSTUM_SCREEN));
	CHECK_EQ(0, frustum_classify(&f, box, 8, FRUSTUM_SCREEN));
	// Only the planes asked about count: aside is inside the near plane
	CHECK_EQ(0, frustum_classify(&f, aside, 1, 1u << FRUSTUM_NEAR));
}

// Just past the screen's right edge, inside the guard band: not on the screen, but nothing to cut
static void the_guard_band_is_wider_than_the_screen(void)
{
	frustum f;
	const frustum_point past_edge[1] = {{-600, -500, 35}};	 // 1.2 x as far aside as in front
	const frustum_point past_guard[1] = {{-1100, -500, 35}}; // 2.2 x

	level_camera(&f);
	CHECK_EQ(FRUSTUM_OUTSIDE, frustum_classify(&f, past_edge, 1, FRUSTUM_SCREEN));
	CHECK_EQ(0, frustum_classify(&f, past_edge, 1, FRUSTUM_GUARD));
	CHECK_EQ(FRUSTUM_OUTSIDE, frustum_classify(&f, past_guard, 1, FRUSTUM_GUARD));
}

// A wall you stand beside: from behind you to far in front, along your right. It needs the near
// plane's cut and the right edge's, and what's left is all in view
static void a_wall_beside_you_is_cut_to_the_view(void)
{
	frustum f;
	const frustum_point wall[4] = {{-40, 300, 0}, {-40, 300, 200}, {-40, -2000, 200}, {-40, -2000, 0}};
	frustum_point out[FRUSTUM_POLY_MAX];
	unsigned cut;
	int n;

	level_camera(&f);
	cut = frustum_classify(&f, wall, 4, FRUSTUM_SCREEN);
	CHECK(cut & (1u << FRUSTUM_NEAR));
	CHECK(cut != FRUSTUM_OUTSIDE);
	n = frustum_clip(&f, cut, wall, 4, out);
	CHECK(n >= 3);
	CHECK(all_inside(&f, FRUSTUM_SCREEN, out, n));
	// The far end's still there, uncut
	{
		bool far_end = false;

		for (int k = 0; k < n; ++k)
			if (out[k].y == -2000 && out[k].z == 200)
				far_end = true;
		CHECK(far_end);
	}
}

// Two faces sharing an edge, listed opposite ways round along it: the corner the cut adds on the
// shared edge comes out the same in both, so no crack shows between them
static void a_shared_edge_is_cut_the_same_in_both(void)
{
	frustum f;
	// Both run from behind you (y 200) to in front (y -800); they share x = 30, z 0 .. 100
	const frustum_point left[4] = {{-30, 200, 0}, {-30, -800, 0}, {30, -800, 0}, {30, 200, 0}};
	const frustum_point right[4] = {{30, 200, 0}, {30, -800, 0}, {30, -800, 100}, {30, 200, 100}};
	frustum_point a[FRUSTUM_POLY_MAX], b[FRUSTUM_POLY_MAX];
	int na, nb, found = 0;

	level_camera(&f);
	na = frustum_clip(&f, 1u << FRUSTUM_NEAR, left, 4, a);
	nb = frustum_clip(&f, 1u << FRUSTUM_NEAR, right, 4, b);
	CHECK_EQ(4, na);
	CHECK_EQ(4, nb);
	// The new corner at x 30, z 0, where the shared edge crosses the near plane: in both
	for (int i = 0; i < na; ++i)
		for (int j = 0; j < nb; ++j)
			if (a[i].x == 30 && a[i].z == 0 && a[i].y > -800 && b[j].x == a[i].x && b[j].y == a[i].y &&
				b[j].z == a[i].z)
				++found;
	CHECK_EQ(1, found);
}

// Looking down 45 degrees: the floor just in front of your feet is on the screen, and the
// horizon's well off its top
static void looking_down_sees_your_feet(void)
{
	frustum f;
	// cos 45 x 1024 = 724: forward is north and down, up is north and up
	const int down_north[3] = {0, -724, -724};
	const int up_north[3] = {0, -724, 724};
	const frustum_point feet[1] = {{0, -30, 0}};
	const frustum_point horizon[1] = {{0, -5000, 35}};

	frustum_set_camera(&f, EYE, down_north, EAST, up_north, FRUSTUM_ONE, 768, 10);
	CHECK_EQ(0, frustum_classify(&f, feet, 1, FRUSTUM_SCREEN));
	CHECK_EQ(FRUSTUM_OUTSIDE, frustum_classify(&f, horizon, 1, FRUSTUM_SCREEN));
}

// A doorway's edge as a plane from the eye: a vertical edge 500 north and 100 east of you. What's
// beyond it and further east than it is hidden; what's through the doorway (west of the edge,
// seen from here) is kept
static void a_portal_edge_is_a_plane_from_the_eye(void)
{
	frustum f;
	unsigned bit;
	const frustum_point beyond_east[1] = {{-400, -1000, 0}};   // further east than the edge's line
	const frustum_point through[1] = {{0, -1000, 0}};		   // straight on, through it
	const frustum_point corners[4] = {{-400, -1000, 0}, {0, -1000, 0}, {0, -1000, 100}, {-400, -1000, 100}};
	frustum_point out[FRUSTUM_POLY_MAX];
	int n;

	level_camera(&f);
	bit = frustum_add_edge(&f, at(-100, -500, 0), at(-100, -500, 100), at(0, -500, 0));
	CHECK_EQ(1u << FRUSTUM_PORTAL_FIRST, bit);
	CHECK_EQ(FRUSTUM_PORTAL_FIRST + 1, f.count);
	CHECK_EQ(FRUSTUM_OUTSIDE, frustum_classify(&f, beyond_east, 1, bit));
	CHECK_EQ(0, frustum_classify(&f, through, 1, bit));
	// A wall across the far side, half of it beyond the edge: cut at x -200 (the eye's line
	// through the edge, twice as far out)
	n = frustum_clip(&f, bit, corners, 4, out);
	CHECK_EQ(4, n);
	for (int k = 0; k < n; ++k)
		CHECK(out[k].x >= -201 && out[k].x <= 0);
	// The eye on the edge's line: no plane
	CHECK_EQ(0, frustum_add_edge(&f, at(0, -500, 35), at(0, -1000, 35), at(0, -500, 50)));
}

// A box's test by its centre and size says exactly what its eight corners say, for boxes all
// round the camera, looking level and down, with a portal's plane too
static void a_box_is_tested_as_its_corners_are(void)
{
	frustum f;
	const int down_north[3] = {0, -724, -724};
	const int up_north[3] = {0, -724, 724};
	int differ = 0, tried = 0;

	for (int view = 0; view < 2; ++view)
	{
		if (view == 0)
			level_camera(&f);
		else
			frustum_set_camera(&f, EYE, down_north, EAST, up_north, FRUSTUM_ONE, 768, 10);
		frustum_add_edge(&f, at(-100, -500, 0), at(-100, -500, 100), at(0, -500, 0));
		for (int x = -1500; x <= 1500; x += 250)
			for (int y = -1500; y <= 1500; y += 250)
				for (int z = -400; z <= 400; z += 200)
				{
					const frustum_point centre = {x, y, z};
					const frustum_point half = {40 + (x & 0x70), 60, 30 + (y & 0x30)};
					frustum_point corners[8];

					for (int k = 0; k < 8; ++k)
					{
						corners[k].x = x + ((k & 1) ? half.x : -half.x);
						corners[k].y = y + ((k & 2) ? half.y : -half.y);
						corners[k].z = z + ((k & 4) ? half.z : -half.z);
					}
					++tried;
					if (frustum_classify_box(&f, centre, half, 0x3FF) != frustum_classify(&f, corners, 8, 0x3FF))
						++differ;
				}
	}
	CHECK(tried > 1000);
	CHECK_EQ(0, differ);
}

// Looking into a corridor through its doorway (level.c grey_portal). The corridor runs north
// from 500 north of you, its side walls at x 150 and x -150 (east is -x); its far end's doorway
// is from x 50 (the near jamb, the end of a wall) to x -150 (the far jamb: the side wall's end).
// You're at x 0, between them, not beyond the side wall at -150. Then the sill (the floor's far
// edge) and the far jamb can't hide any of the floor: it's inside both, so level.c leaves them
// out of the floor's tests. The near jamb does hide some: the floor behind the wall beside it
static void a_doorway_s_sill_and_far_jamb_hide_no_floor(void)
{
	frustum f;
	const frustum_point in = {-50, -500, 100}; // in the opening
	unsigned near_jamb, far_jamb, sill;
	int inside_all = 0, tried = 0, near_hides = 0;

	level_camera(&f);
	near_jamb = frustum_add_edge(&f, at(50, -500, 0), at(50, -500, 1000), in);
	far_jamb = frustum_add_edge(&f, at(-150, -500, 0), at(-150, -500, 1000), in);
	sill = frustum_add_edge(&f, at(50, -500, 0), at(-150, -500, 0), in);
	CHECK(near_jamb != 0 && far_jamb != 0 && sill != 0);
	for (int x = -140; x <= 140; x += 20)
		for (int y = -510; y >= -3000; y -= 100)
		{
			const frustum_point p[1] = {{x, y, 0}};

			++tried;
			if (frustum_classify(&f, p, 1, far_jamb | sill) == 0)
				++inside_all;
			if (frustum_classify(&f, p, 1, near_jamb) == FRUSTUM_OUTSIDE)
				++near_hides;
		}
	CHECK_EQ(tried, inside_all);
	CHECK(near_hides > 0);
	// Below the floor, the sill does hide (seen from inside, the city's roofs under it)
	{
		const frustum_point under[1] = {{-50, -1000, -200}};

		CHECK_EQ(FRUSTUM_OUTSIDE, frustum_classify(&f, under, 1, sill));
	}
}

// Wholly outside after a cut: nothing comes back
static void a_polygon_behind_you_is_dropped(void)
{
	frustum f;
	const frustum_point behind[4] = {{-50, 100, 0}, {50, 100, 0}, {50, 200, 0}, {-50, 200, 0}};
	frustum_point out[FRUSTUM_POLY_MAX];

	level_camera(&f);
	CHECK_EQ(0, frustum_clip(&f, FRUSTUM_SCREEN, behind, 4, out));
}

// In a mirror across x = 300: a point's as far inside the reflected view's planes as its
// reflection is inside the view's own, for every plane, and the eye's reflected
static void a_mirror_reflects_the_view(void)
{
	frustum f, m;
	static const frustum_point points[4] = {{0, -500, 35}, {200, -300, 80}, {-400, -900, 0}, {900, -100, 300}};

	level_camera(&f);
	frustum_reflect(&f, 0, 300, &m);
	CHECK_EQ(600, m.eye.x);
	CHECK_EQ(EYE.y, m.eye.y);
	CHECK_EQ(-NORTH[0], m.forward[0]);
	CHECK_EQ(-EAST[0], m.right[0]); // the mirror swaps left and right
	for (int k = 0; k < 4; ++k)
	{
		const frustum_point p = points[k];
		const frustum_point r = {600 - p.x, p.y, p.z};

		for (int i = 0; i < f.count; ++i)
			CHECK_EQ(frustum_distance(&f, i, p), frustum_distance(&m, i, r));
	}
	// Across y, the same
	frustum_reflect(&f, 1, -1000, &m);
	CHECK_EQ(-2000, m.eye.y);
	CHECK_EQ(-NORTH[1], m.forward[1]);
	for (int k = 0; k < 4; ++k)
	{
		const frustum_point p = points[k];
		const frustum_point r = {p.x, -2000 - p.y, p.z};

		for (int i = 0; i < f.count; ++i)
			CHECK_EQ(frustum_distance(&f, i, p), frustum_distance(&m, i, r));
	}
}

void run_frustum_tests(void)
{
	RUN(a_mirror_reflects_the_view);
	RUN(square_roots_round_down);
	RUN(distances_are_in_world_units);
	RUN(what_the_camera_sees);
	RUN(the_guard_band_is_wider_than_the_screen);
	RUN(a_wall_beside_you_is_cut_to_the_view);
	RUN(a_shared_edge_is_cut_the_same_in_both);
	RUN(looking_down_sees_your_feet);
	RUN(a_portal_edge_is_a_plane_from_the_eye);
	RUN(a_box_is_tested_as_its_corners_are);
	RUN(a_doorway_s_sill_and_far_jamb_hide_no_floor);
	RUN(a_polygon_behind_you_is_dropped);
}
