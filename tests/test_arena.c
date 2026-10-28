#include "test.h"
#include "../src/game/arena.h"
#include "../src/game/cycle.h"

#define VIEW_RANGE_ISO_TEST 2000 // as VIEW_CULL_ISO_RANGE

static void default_arena_is_8x_the_old_one(void)
{
	CHECK_EQ(ARENA_PIECES_DEFAULT, arena_pieces());
	CHECK_EQ(8000, arena_half());
	CHECK_EQ(8000 - ARENA_WALL_CLEARANCE, arena_player_limit());
}

static void size_is_clamped_to_its_range(void)
{
	CHECK_EQ(ARENA_PIECES_MIN, arena_set_pieces(0));
	CHECK_EQ(ARENA_PIECES_MAX, arena_set_pieces(1000));
	CHECK(arena_half() < WORLD_COORD_LIMIT); // a bounded cycle can never reach the short overflow guard
	arena_set_pieces(ARENA_PIECES_DEFAULT);
}

// North side: columns start at the west end (+x = 8000) and run east (-x)
static void draws_only_pieces_in_view(void)
{
	int first = -1;
	int count = arena_visible_pieces(8000, -1, -WORLD_VIEW_RANGE, WORLD_VIEW_RANGE, &first); // view centred on x = 0

	CHECK(count > 0);
	CHECK(count <= 2 * WORLD_VIEW_RANGE / (2 * ARENA_PIECE_HALF) + 2);
	CHECK(count < ARENA_PIECES_DEFAULT); // not the whole side
	CHECK_EQ(32, first + count / 2);	 // centred on the middle piece (8000 / 250 = 32)
}

static void window_stops_at_the_side_ends(void)
{
	int first = -1;
	int count = arena_visible_pieces(8000, -1, 7900 - WORLD_VIEW_RANGE, 7900 + WORLD_VIEW_RANGE, &first); // west end
	CHECK_EQ(0, first);
	CHECK(count > 0);

	count = arena_visible_pieces(8000, -1, -7900 - WORLD_VIEW_RANGE, -7900 + WORLD_VIEW_RANGE, &first); // east end
	CHECK_EQ(ARENA_PIECES_DEFAULT, first + count); // the last piece is included, nothing past it
}

static void span_beyond_the_side_draws_nothing(void)
{
	int first;
	CHECK_EQ(0, arena_visible_pieces(8000, -1, 9000, 15000, &first));
	CHECK_EQ(0, arena_visible_pieces(8000, -1, -15000, -9000, &first));
}

static void narrow_view_draws_few_pieces(void)
{
	int first;
	int count = arena_visible_pieces(8000, -1, -VIEW_RANGE_ISO_TEST, VIEW_RANGE_ISO_TEST, &first);
	CHECK(count <= 2 * VIEW_RANGE_ISO_TEST / (2 * ARENA_PIECE_HALF) + 2);
}

static void small_arena_draws_whole_sides(void)
{
	int first = -1;
	arena_set_pieces(ARENA_PIECES_MIN);
	CHECK_EQ(ARENA_PIECES_MIN, arena_visible_pieces(arena_half(), -1, -WORLD_VIEW_RANGE, WORLD_VIEW_RANGE, &first));
	CHECK_EQ(0, first);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
}

static void bounded_cycle_follows_a_resized_arena(void)
{
	cycle c;
	arena_set_pieces(ARENA_PIECES_MIN);
	cycle_init(&c, 0, 5000, HEADING_SOUTH); // outside the shrunk arena
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(arena_player_limit(), c.position.y);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
}

static void cruise_speed_is_tunable(void)
{
	cycle c;
	cycle_init(&c, 0, 0, HEADING_SOUTH);
	CHECK_EQ(20, cycle_set_cruise_speed(20));
	for (int i = 0; i < 30; ++i)
		cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(20, c.speed); // ramps up to the new cruise

	cycle_set_cruise_speed(5);
	cycle_update(&c, 0, NULL, 0);
	CHECK_EQ(5, c.speed); // tuning down takes effect at once

	CHECK_EQ(1, cycle_set_cruise_speed(0));
	CHECK_EQ(CYCLE_CRUISE_MAX, cycle_set_cruise_speed(255));
	cycle_set_cruise_speed(CYCLE_BASE_SPEED);
}

// Columns for the whole north side (x 8000 -> -8000), eye at the centre: 8000 from the wall
static short along[ARENA_WALL_COLUMNS_MAX];

static void far_wall_is_drawn_in_long_pieces(void)
{
	int n = arena_wall_columns(8000, -1, 0, ARENA_PIECES_DEFAULT, 0, 8000, along, ARENA_WALL_COLUMNS_MAX);

	CHECK_EQ(8000, along[0]);	   // starts at the side's first column...
	CHECK_EQ(-8000, along[n - 1]); // ...and ends at its last
	CHECK(n <= ARENA_PIECES_DEFAULT / 8 + 1); // 8000 away: every quad is 8 pieces (2000)
	for (int i = 0; i < n; ++i)
		CHECK_EQ(0, along[i] % (2 * ARENA_PIECE_HALF)); // every column on the 250 grid
}

static void near_wall_keeps_full_detail_beside_the_eye(void)
{
	// Eye 100 from the wall at x = 0: pieces within ARENA_LOD_NEAR are each drawn
	int n = arena_wall_columns(8000, -1, 0, ARENA_PIECES_DEFAULT, 0, 100, along, ARENA_WALL_COLUMNS_MAX);
	int near_steps = 0;

	CHECK(n <= ARENA_WALL_COLUMNS_MAX);
	CHECK_EQ(-8000, along[n - 1]);
	for (int i = 1; i < n; ++i)
	{
		int step = along[i - 1] - along[i];
		int nearer = along[i] < 0 ? -along[i - 1] : along[i]; // distance of the nearer end from x = 0
		if (nearer < 0)
			nearer = 0; // the piece spans the eye
		CHECK(step >= 2 * ARENA_PIECE_HALF);
		if (nearer < ARENA_LOD_NEAR)
		{
			CHECK_EQ(2 * ARENA_PIECE_HALF, step); // full detail near the eye
			++near_steps;
		}
	}
	CHECK(near_steps >= 2 * ARENA_LOD_NEAR / (2 * ARENA_PIECE_HALF) - 2);
}

static void largest_arena_fits_the_column_buffer(void)
{
	arena_set_pieces(ARENA_PIECES_MAX);
	// Eye beside the middle of a side: the most near detail plus far pieces both ways
	int n = arena_wall_columns(arena_half(), -1, 0, ARENA_PIECES_MAX, 0, 100, along, ARENA_WALL_COLUMNS_MAX);
	CHECK(n < ARENA_WALL_COLUMNS_MAX); // fitted with room to spare, not cut short
	CHECK_EQ(-arena_half(), along[n - 1]);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
}

void run_arena_tests(void)
{
	RUN(default_arena_is_8x_the_old_one);
	RUN(size_is_clamped_to_its_range);
	RUN(draws_only_pieces_in_view);
	RUN(window_stops_at_the_side_ends);
	RUN(span_beyond_the_side_draws_nothing);
	RUN(narrow_view_draws_few_pieces);
	RUN(small_arena_draws_whole_sides);
	RUN(bounded_cycle_follows_a_resized_arena);
	RUN(cruise_speed_is_tunable);
	RUN(far_wall_is_drawn_in_long_pieces);
	RUN(near_wall_keeps_full_detail_beside_the_eye);
	RUN(largest_arena_fits_the_column_buffer);
}
