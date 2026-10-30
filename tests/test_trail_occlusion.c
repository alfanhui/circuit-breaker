#include "test.h"
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
	// Running across, not alongside
	one_occluder(pt(-1000, -500), pt(1000, -500));
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

	without = trail_build_columns(&far, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);
	with = trail_build_columns(&far, 0, pt(0, 0), &view, &occ, columns, TRAIL_DRAW_COLUMNS_MAX);
	CHECK(without > 2);
	CHECK_EQ(0, with); // wholly behind the nearer trail
	// The nearer trail itself is all drawn
	CHECK_EQ(trail_build_columns(&near, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX),
			 trail_build_columns(&near, 0, pt(0, 0), &view, &occ, columns, TRAIL_DRAW_COLUMNS_MAX));
}

void run_trail_occlusion_tests(void)
{
	RUN(nearer_parallel_trail_hides_the_farther);
	RUN(a_short_one_hides_the_middle);
	RUN(works_whichever_way_the_trail_runs);
	RUN(a_trail_beside_the_eye_hides_what_is_behind_it);
	RUN(farther_or_other_side_or_crossing_hides_nothing);
	RUN(hidden_pieces_are_not_built);
}
