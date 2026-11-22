#include "test.h"
#include <stdbool.h>
#include "../src/game/trail_occlusion.h"
#include "../src/game/trail_pieces.h"

// Remember the directions: north is -y, west is +x (world.h). The eye is at the origin
static world_point pt(short x, short y)
{
	world_point p;
	p.x = x;
	p.y = y;
	return p;
}

static trail_occluders occ;
static int starts[TRAIL_VISIBLE_PARTS_MAX];
static int ends[TRAIL_VISIBLE_PARTS_MAX];

// One occluder: a trail from a to b, seen from the origin
static void one_occluder(world_point a, world_point b)
{
	static trail t;
	world_rect everywhere;

	everywhere.x_min = -9000;
	everywhere.x_max = 9000;
	everywhere.y_min = -9000;
	everywhere.y_max = 9000;
	trail_reset(&t, a);
	trail_set_head(&t, b);
	trail_occluders_begin(&occ, pt(0, 0));
	trail_occluders_add(&occ, &t, 0, &everywhere);
}

// The far trail: 3000 long, 300 to the west (+x), running north (-y) from level with the eye
static int far_parts(void)
{
	return trail_occluders_visible(&occ, pt(300, 0), pt(300, -3000), 0, 3000, starts, ends, TRAIL_VISIBLE_PARTS_MAX);
}

// A nearer trail 100 to the west, from 100 to 1000 ahead. Seen from the eye it covers the far
// trail (3x further across) from 300 to 3000 ahead: only its first 300 still shows
static void nearer_parallel_trail_hides_the_farther(void)
{
	one_occluder(pt(100, -100), pt(100, -1000));
	CHECK_EQ(1, far_parts());
	CHECK_EQ(0, starts[0]);
	CHECK_EQ(300, ends[0]);
}

// A shorter nearer trail, 100 to 500 ahead, hides the far trail's middle, 300 to 1500
static void a_short_one_hides_the_middle(void)
{
	one_occluder(pt(100, -100), pt(100, -500));
	CHECK_EQ(2, far_parts());
	CHECK_EQ(0, starts[0]);
	CHECK_EQ(300, ends[0]);
	CHECK_EQ(1500, starts[1]);
	CHECK_EQ(3000, ends[1]);
}

// The same, with the far trail laid the other way (south): the parts are in its own steps
static void works_whichever_way_the_trail_runs(void)
{
	one_occluder(pt(100, -100), pt(100, -500));
	CHECK_EQ(2, trail_occluders_visible(&occ, pt(300, -3000), pt(300, 0), 0, 3000, starts, ends,
										TRAIL_VISIBLE_PARTS_MAX));
	CHECK_EQ(0, starts[0]); // from its start, 3000 ahead, back to 1500 ahead
	CHECK_EQ(1500, ends[0]);
	CHECK_EQ(2700, starts[1]); // and from 300 ahead to level with the eye
	CHECK_EQ(3000, ends[1]);
}

// A trail running past the eye (from behind it to ahead) hides at least as much as it covers
static void a_trail_beside_the_eye_hides_what_is_behind_it(void)
{
	one_occluder(pt(100, 500), pt(100, -1000));
	CHECK_EQ(0, far_parts()); // all of the far trail, 0 to 3000 ahead
}

static void farther_or_other_side_or_crossing_hides_nothing(void)
{
	// Farther across than the far trail: it's behind it, not in front
	one_occluder(pt(500, -100), pt(500, -3000));
	CHECK_EQ(1, far_parts());
	CHECK_EQ(3000, ends[0] - starts[0]);
	// On the other side of the eye
	one_occluder(pt(-100, -100), pt(-100, -3000));
	CHECK_EQ(1, far_parts());
	CHECK_EQ(3000, ends[0] - starts[0]);
	// Running across, but only on the other side of the eye's line (x -1000 to -100): the line
	// of sight to the far trail (at x 300) never passes it
	one_occluder(pt(-1000, -500), pt(-100, -500));
	CHECK_EQ(1, far_parts());
	CHECK_EQ(3000, ends[0] - starts[0]);
	// On the eye's own line (the player's own trail, behind it)
	one_occluder(pt(0, 100), pt(0, 1000));
	CHECK_EQ(1, far_parts());
	CHECK_EQ(3000, ends[0] - starts[0]);
}

// Drawing: the hidden pieces of a trail aren't built at all
static void hidden_pieces_are_not_built(void)
{
	static trail near;
	static trail far;
	static trail_column columns[TRAIL_DRAW_COLUMNS_MAX];
	world_rect view;
	int without;
	int with;

	view.x_min = -6000;
	view.x_max = 6000;
	view.y_min = -6000;
	view.y_max = 300;
	trail_reset(&near, pt(100, 500));
	trail_set_head(&near, pt(100, -3000));
	trail_reset(&far, pt(300, 0));
	trail_set_head(&far, pt(300, -3000));
	trail_occluders_begin(&occ, pt(0, 0));
	trail_occluders_add(&occ, &near, 0, &view);
	trail_occluders_add(&occ, &far, 0, &view);
	CHECK_EQ(2, occ.count);

	without = trail_build_columns(&far, 0, pt(0, 0), &view, NULL, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);
	with = trail_build_columns(&far, 0, pt(0, 0), &view, &occ, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);
	CHECK(without > 2);
	CHECK_EQ(0, with); // wholly behind the nearer trail
	// The nearer trail itself is all drawn
	CHECK_EQ(trail_build_columns(&near, 0, pt(0, 0), &view, NULL, NULL, columns, TRAIL_DRAW_COLUMNS_MAX),
			 trail_build_columns(&near, 0, pt(0, 0), &view, &occ, NULL, columns, TRAIL_DRAW_COLUMNS_MAX));
}

// The hidden range comes from slopes worked out once per near segment (no division per pair).
// Over many layouts, it must never hide more than exact division does (rounded inwards: what's
// visible is always drawn), and lose at most a unit at each end against it
static void slopes_hide_what_division_does(void)
{
	int worst = 0, never_more = 1;

	for (int d1 = 7; d1 < 4000; d1 += 173)
		for (int d2 = d1 + 1; d2 < 9000; d2 += 611)
			for (int a = 40; a < 6000; a += 997)
			{
				const int b = a + 731;
				// Exact, as the division per pair did it: A * d2 / d1 rounded up, B * d2 / d1 down
				const int t0 = (a * d2 + d1 - 1) / d1;
				const int t1 = b * d2 / d1;
				const int far_end = 9000; // the far trail runs north from level with the eye

				int parts;

				one_occluder(pt((short)d1, (short)-a), pt((short)d1, (short)-b));
				parts = trail_occluders_visible(&occ, pt((short)d2, 0), pt((short)d2, (short)-far_end), 0, far_end, starts,
												ends, TRAIL_VISIBLE_PARTS_MAX);
				if (parts < 1 || t1 >= far_end)
					continue; // hidden past the far trail's end: only one part, checked elsewhere
				// The first visible part runs from 0 to where it's hidden from: t0, or a little later
				never_more &= ends[0] >= t0;
				if (ends[0] - t0 > worst)
					worst = ends[0] - t0;
				// And the next starts where it's hidden to: t1, or a little earlier
				CHECK_EQ(2, parts);
				if (parts == 2)
				{
					never_more &= starts[1] <= t1;
					if (t1 - starts[1] > worst)
						worst = t1 - starts[1];
				}
			}
	CHECK(never_more);
	CHECK(worst <= 1);
}

// A pillar's face hides a trail behind it, as a nearer trail would; only the faces turned to the
// eye are listed, and a block the eye is level with on one axis lists just one
static void a_block_hides_what_is_behind_it(void)
{
	world_rect everywhere = {-9000, -9000, 9000, 9000};
	world_rect pillar = {100, -1000, 400, -100}; // to the west (+x) and ahead (north, -y)
	int n;

	trail_occluders_begin(&occ, pt(0, 0));
	trail_occluders_add_blocks(&occ, &pillar, 1, &everywhere);
	CHECK_EQ(2, occ.count); // its east face (x 100) and its south face (y -100)
	// The far trail, 300 west, running north: behind the east face from 300 to 3000 ahead
	n = far_parts();
	CHECK_EQ(1, n);
	CHECK_EQ(0, starts[0]);
	CHECK(ends[0] <= 300);
	// Level with the eye across x (it spans x = 0): only its south face can hide anything
	pillar.x_min = -200;
	trail_occluders_begin(&occ, pt(0, 0));
	trail_occluders_add_blocks(&occ, &pillar, 1, &everywhere);
	CHECK_EQ(1, occ.count);
	CHECK_EQ(1, occ.along_x[0]);
}

/*
 * Running across (the user, 2026-11-18: sections of trails across the view weren't culled). A trail
 * across the view 500 ahead, from x -1000 to 1000, hides all of the far trail (x 300, running
 * north) past it: only its first 500 or so shows. The hidden range is rounded inwards: a unit more
 * may show, never less
 */
static void a_trail_across_hides_what_runs_on_behind_it(void)
{
	one_occluder(pt(-1000, -500), pt(1000, -500));
	CHECK_EQ(1, far_parts());
	CHECK_EQ(0, starts[0]);
	CHECK(ends[0] >= 500 && ends[0] <= 503);
}

// A shorter one across, x 100 to 200, 500 ahead: the far trail (x 300) is behind it where the line
// of sight crosses y -500 between x 100 and 200: from 300 x 500 / 200 = 750 to 300 x 500 / 100 =
// 1500 ahead. Both ends rounded inwards
static void a_short_one_across_hides_a_stretch(void)
{
	one_occluder(pt(100, -500), pt(200, -500));
	CHECK_EQ(2, far_parts());
	CHECK_EQ(0, starts[0]);
	CHECK(ends[0] >= 750 && ends[0] <= 752);
	CHECK(starts[1] >= 1498 && starts[1] <= 1500);
	CHECK_EQ(3000, ends[1]);
	// A far trail on the eye's other side (x -300) isn't behind it at all
	CHECK_EQ(1, trail_occluders_visible(&occ, pt(-300, 0), pt(-300, -3000), 0, 3000, starts, ends,
										TRAIL_VISIBLE_PARTS_MAX));
	CHECK_EQ(3000, ends[0] - starts[0]);
}

// The rule against lines of sight, over many layouts: a point of a trail running across it is
// left out only if the straight line from the eye to it crosses the near trail before it
static void across_never_hides_what_is_seen(void)
{
	int checked = 0;

	for (int a = -900; a <= 900; a += 150)
		for (int len = 50; len <= 1200; len += 275)
			for (int d1 = 60; d1 <= 1500; d1 += 290)
			{
				one_occluder(pt((short)a, (short)-d1), pt((short)(a + len), (short)-d1));
				for (int c = -1400; c <= 1400; c += 233)
				{
					const int n = trail_occluders_visible(&occ, pt((short)c, 0), pt((short)c, -4000), 0, 4000, starts, ends,
														  TRAIL_VISIBLE_PARTS_MAX);

					for (int s = 0; s <= 4000; s += 37)
					{
						bool shown = false;

						for (int k = 0; k < n; ++k)
							shown = shown || (s >= starts[k] && s <= ends[k]);
						if (!shown)
						{
							// Left out: the line from (0, 0) to (c, -s) must cross y = -d1 within the near
							// trail, before it (exactly: c x d1 / s between a and a + len, and s > d1)
							const long long x_cross_s = (long long)c * d1; // x where it crosses, times s
							const bool hidden = s > d1 && x_cross_s >= (long long)a * s && x_cross_s <= (long long)(a + len) * s;

							CHECK(hidden);
							++checked;
						}
					}
				}
			}
	CHECK(checked > 1000);
}

void run_trail_occlusion_tests(void)
{
	RUN(a_trail_across_hides_what_runs_on_behind_it);
	RUN(a_short_one_across_hides_a_stretch);
	RUN(across_never_hides_what_is_seen);
	RUN(slopes_hide_what_division_does);
	RUN(nearer_parallel_trail_hides_the_farther);
	RUN(a_short_one_hides_the_middle);
	RUN(works_whichever_way_the_trail_runs);
	RUN(a_trail_beside_the_eye_hides_what_is_behind_it);
	RUN(farther_or_other_side_or_crossing_hides_nothing);
	RUN(hidden_pieces_are_not_built);
	RUN(a_block_hides_what_is_behind_it);
}
