#include "test.h"
#include "../src/game/trail_pieces.h"

static world_point pt(int x, int y)
{
	world_point p;
	p.x = (short)x;
	p.y = (short)y;
	return p;
}

static int abs_i(int v)
{
	return v < 0 ? -v : v;
}

static int cheb(world_point a, world_point b)
{
	int dx = abs_i(a.x - b.x), dy = abs_i(a.y - b.y);
	return dx > dy ? dx : dy;
}

// A trail with one live segment from start to head
static void make_line(trail *t, world_point start, world_point head)
{
	trail_reset(t, start);
	trail_set_head(t, head);
}

static trail_column columns[TRAIL_DRAW_COLUMNS_MAX];

// The square the old fixed-range culling used: focus +- WORLD_VIEW_RANGE
static world_rect square(world_point c)
{
	world_rect r;
	r.x_min = (short)(c.x - WORLD_VIEW_RANGE);
	r.x_max = (short)(c.x + WORLD_VIEW_RANGE);
	r.y_min = (short)(c.y - WORLD_VIEW_RANGE);
	r.y_max = (short)(c.y + WORLD_VIEW_RANGE);
	return r;
}

static void short_trail_is_cut_into_near_pieces(void)
{
	trail t;
	int n;

	make_line(&t, pt(0, 0), pt(0, -1000));
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);

	// Near pieces are cut on a grid centred on the player (one piece straddles it): boundaries
	// at y = -128, -384, -640, -896, so the ends make 5 pieces, 6 columns
	CHECK_EQ(6, n);
	CHECK_EQ(-128, columns[1].p.y);
	CHECK_EQ(-384, columns[2].p.y);
	CHECK_EQ(0, columns[0].joined);
	CHECK_EQ(1, columns[n - 1].joined);
	CHECK_EQ(-1000, columns[n - 1].p.y); // ends exactly at the head
}

// SGL sorts each piece by its centre (VDP1 has no depth buffer). The piece beside the player
// has its centre level with it, so the cycle and that piece are judged at the same point along
// the trail: drawn in the right order whichever side of the trail the camera is (iso_slide.steps)
static void piece_beside_the_player_is_centred_on_it(void)
{
	trail t;
	int n;
	int found = 0;

	make_line(&t, pt(0, 1000), pt(0, -1000));
	world_rect view = square(pt(25, 300));
	n = trail_build_columns(&t, 0, pt(25, 300), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX); // 25 beside it
	for (int i = 1; i < n; ++i)
		if (columns[i - 1].p.y == 300 + TRAIL_NEAR_PIECE / 2 && columns[i].p.y == 300 - TRAIL_NEAR_PIECE / 2)
			found = 1;
	CHECK(found);

	// Running the other way along it: still centred
	make_line(&t, pt(0, -1000), pt(0, 1000));
	n = trail_build_columns(&t, 0, pt(25, 300), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);
	found = 0;
	for (int i = 1; i < n; ++i)
		if (columns[i - 1].p.y == 300 - TRAIL_NEAR_PIECE / 2 && columns[i].p.y == 300 + TRAIL_NEAR_PIECE / 2)
			found = 1;
	CHECK(found);
}

static void long_trail_is_clipped_to_the_draw_window(void)
{
	trail t;
	int n;

	make_line(&t, pt(0, 15000), pt(0, -15000)); // runs right through the player
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);

	CHECK(n > 2);
	for (int i = 0; i < n; ++i)
		CHECK(cheb(columns[i].p, pt(0, 0)) <= WORLD_VIEW_RANGE);
	CHECK_EQ(WORLD_VIEW_RANGE, columns[0].p.y);			// clipped at the window edge...
	CHECK_EQ(-WORLD_VIEW_RANGE, columns[n - 1].p.y);	// ...on both sides
}

static void pieces_are_short_near_and_longer_far(void)
{
	trail t;
	int n;

	make_line(&t, pt(0, 15000), pt(0, -15000));
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);

	for (int i = 1; i < n; ++i)
	{
		int piece = cheb(columns[i - 1].p, columns[i].p);
		int near = cheb(columns[i - 1].p, pt(0, 0)) < TRAIL_NEAR_RANGE ||
				   cheb(columns[i].p, pt(0, 0)) < TRAIL_NEAR_RANGE;
		CHECK(piece <= (near ? TRAIL_NEAR_PIECE : TRAIL_FAR_PIECE));
	}
	CHECK(n < 2 * WORLD_VIEW_RANGE / TRAIL_NEAR_PIECE); // far pieces really are longer
}

static void trail_out_of_range_draws_nothing(void)
{
	trail t;
	make_line(&t, pt(7000, 7000), pt(7000, -7000)); // parallel to the player, 7000 away
	world_rect view = square(pt(0, 0));
	CHECK_EQ(0, trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX));
}

static void diagonal_trail_stays_on_its_line(void)
{
	trail t;
	int n;

	make_line(&t, pt(-9000, -3000), pt(9000, 3000)); // slope 1/3, as the stress fill's head can be
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);

	CHECK(n > 2);
	for (int i = 0; i < n; ++i)
		CHECK(abs_i(columns[i].p.x - 3 * columns[i].p.y) <= 3); // y = x / 3, give or take rounding
}

static void each_segment_starts_its_own_strip_newest_first(void)
{
	trail t;
	int n, strips = 0;

	trail_reset(&t, pt(0, 0));
	trail_set_head(&t, pt(0, -500));
	trail_add_corner(&t);
	trail_set_head(&t, pt(-500, -500)); // newest: heading east (-x)
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);

	for (int i = 0; i < n; ++i)
		strips += !columns[i].joined;
	CHECK_EQ(2, strips);
	CHECK_EQ(-500, columns[0].p.y); // first strip is the newest segment, along y = -500
	CHECK_EQ(0, columns[0].p.x);
}

static void column_limit_keeps_the_newest(void)
{
	trail t;
	int n;

	make_line(&t, pt(0, 5000), pt(0, -5000));
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, 6);
	CHECK_EQ(6, n);
	CHECK_EQ(0, columns[0].joined);
	CHECK_EQ(5000, columns[0].p.y); // starts at the segment's beginning, stops when full
}

static void head_gap_is_respected(void)
{
	trail t;
	int n;

	make_line(&t, pt(0, 0), pt(0, -1000));
	world_rect view = square(pt(0, -1000));
	n = trail_build_columns(&t, 25, pt(0, -1000), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);
	CHECK_EQ(-975, columns[n - 1].p.y); // the wall stops 25 behind the cycle
}

// 1st person camera at the origin facing north (-y)
static wall_clip_camera north_camera(void)
{
	wall_clip_camera c;
	c.eye = pt(0, 0);
	c.forward_x = 0;
	c.forward_y = -1024;
	c.view_width = WALL_CLIP_VIEW_WIDTH_90;
	return c;
}

static int forward_of(world_point p)
{
	return -p.y; // facing north from the origin
}

// A trail running alongside the camera, 100 to its side: it starts behind and runs on ahead
static void trail_beside_the_camera_starts_in_front(void)
{
	trail t;
	wall_clip_camera c = north_camera();
	int n;

	make_line(&t, pt(100, 3000), pt(100, -3000));
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);
	n = trail_clip_columns(columns, n, &c, TRAIL_MIN_FORWARD, columns);

	CHECK(n >= 2);
	CHECK_EQ(0, columns[0].joined);
	for (int i = 0; i < n; ++i)
		CHECK(forward_of(columns[i].p) >= TRAIL_MIN_FORWARD); // no corner behind the camera
	CHECK_EQ(TRAIL_MIN_FORWARD, forward_of(columns[0].p));	 // the near end slid up to the limit
	CHECK_EQ(100, columns[0].p.x);							 // still on the trail
	CHECK_EQ(-3000, columns[n - 1].p.y);					 // and it runs on to the head
}

static void trail_behind_the_camera_is_dropped(void)
{
	trail t;
	wall_clip_camera c = north_camera();
	int n;

	make_line(&t, pt(100, 500), pt(100, 3000)); // all behind (+y)
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);
	CHECK_EQ(0, trail_clip_columns(columns, n, &c, TRAIL_MIN_FORWARD, columns));
}

static void strips_stay_separate_after_clipping(void)
{
	trail t;
	wall_clip_camera c = north_camera();
	int n, strips = 0;

	// Behind and to the side, then a turn running away ahead: two strips, the first dropped
	trail_reset(&t, pt(-500, 1000));
	trail_set_head(&t, pt(-500, -200));
	trail_add_corner(&t);
	trail_set_head(&t, pt(-500, -2000));
	trail_add_corner(&t);
	trail_set_head(&t, pt(500, -2000)); // newest: crossing ahead of the camera
	world_rect view = square(pt(0, 0));
	n = trail_build_columns(&t, 0, pt(0, 0), &view, NULL, columns, TRAIL_DRAW_COLUMNS_MAX);
	n = trail_clip_columns(columns, n, &c, TRAIL_MIN_FORWARD, columns);

	for (int i = 0; i < n; ++i)
	{
		strips += !columns[i].joined;
		CHECK(forward_of(columns[i].p) >= TRAIL_MIN_FORWARD - 1);
	}
	// The crossing and the run ahead. The first leg's front part (20..200 ahead) is 500 to the
	// side: outside the 90 degree view, so it goes too
	CHECK_EQ(2, strips);
	CHECK_EQ(0, columns[0].joined);
}

void run_trail_pieces_tests(void)
{
	RUN(short_trail_is_cut_into_near_pieces);
	RUN(piece_beside_the_player_is_centred_on_it);
	RUN(long_trail_is_clipped_to_the_draw_window);
	RUN(pieces_are_short_near_and_longer_far);
	RUN(trail_out_of_range_draws_nothing);
	RUN(diagonal_trail_stays_on_its_line);
	RUN(each_segment_starts_its_own_strip_newest_first);
	RUN(column_limit_keeps_the_newest);
	RUN(head_gap_is_respected);
	RUN(trail_beside_the_camera_starts_in_front);
	RUN(trail_behind_the_camera_is_dropped);
	RUN(strips_stay_separate_after_clipping);
}
