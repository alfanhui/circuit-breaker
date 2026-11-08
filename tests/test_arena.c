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


#include "../src/game/collision.h"

// Is p inside the block grown by `by` on every side?
static int inside_grown(world_point p, const world_rect *b, int by)
{
	return p.x > b->x_min - by && p.x < b->x_max + by && p.y > b->y_min - by && p.y < b->y_max + by;
}

static void each_layout_has_its_blocks(void)
{
	int count;
	const world_rect *b;

	arena_set_layout(ARENA_OPEN);
	arena_blocks(&count);
	CHECK_EQ(0, count);
	CHECK_EQ(1000, arena_unit()); // an eighth of 8000

	arena_set_layout(ARENA_DONUT);
	b = arena_blocks(&count);
	CHECK_EQ(1, count);
	CHECK_EQ(-3000, b[0].x_min);
	CHECK_EQ(3000, b[0].y_max);

	arena_set_layout(ARENA_PILLARS);
	b = arena_blocks(&count);
	CHECK_EQ(4, count);
	for (int i = 0; i < count; ++i)
		CHECK_EQ(3000, b[i].x_max - b[i].x_min); // 3 units square

	arena_set_layout(ARENA_ROOMS);
	arena_blocks(&count);
	CHECK_EQ(6, count);
	CHECK(count <= ARENA_BLOCKS_MAX);
	arena_set_layout(ARENA_OPEN);
}

// Every block's faces are whole wall pieces, at every size (level.c draws them piece by piece)
static void blocks_are_whole_pieces_at_every_size(void)
{
	for (int layout = 0; layout < ARENA_LAYOUT_COUNT; ++layout)
		for (int pieces = ARENA_PIECES_MIN; pieces <= ARENA_PIECES_MAX; pieces += ARENA_PIECES_STEP)
		{
			int count;
			const world_rect *b;

			arena_set_pieces(pieces);
			arena_set_layout(layout);
			b = arena_blocks(&count);
			for (int i = 0; i < count; ++i)
			{
				CHECK_EQ(0, (b[i].x_max - b[i].x_min) % (2 * ARENA_PIECE_HALF));
				CHECK_EQ(0, (b[i].y_max - b[i].y_min) % (2 * ARENA_PIECE_HALF));
				CHECK(b[i].x_min >= -arena_half() && b[i].x_max <= arena_half());
				CHECK(b[i].y_min >= -arena_half() && b[i].y_max <= arena_half());
			}
		}
	arena_set_pieces(ARENA_PIECES_DEFAULT);
	arena_set_layout(ARENA_OPEN);
}

// The rooms' cross reaches the outer walls, even when the unit's rounded down: no way round
static void rooms_are_closed_but_for_the_doors(void)
{
	world_point from = {-8500, -4000}; // the north-east room's east side (east is -x, north -y)
	int count;
	const world_rect *b;

	arena_set_pieces(72); // half 9000: the unit rounds down to 1000, so 8 units falls short
	arena_set_layout(ARENA_ROOMS);
	b = arena_blocks(&count);
	CHECK_EQ(-arena_half(), b[0].y_min);
	CHECK_EQ(arena_half(), b[5].x_max);
	// Riding south (+y) down the room's east side, the cross's east arm stops it short
	CHECK(collision_arena(from, HEADING_SOUTH, 10000, 0, 0) < 4000);
	// Through the east arm's door, 3 to 5 units out, it gets through
	from.x = -4000;
	CHECK_EQ(6000, collision_arena(from, HEADING_SOUTH, 6000, 0, 0));
	arena_set_pieces(ARENA_PIECES_DEFAULT);
	arena_set_layout(ARENA_OPEN);
}

// You start clear of every block, with room ahead to see where you are before you have to turn
static void every_start_is_clear_with_room_ahead(void)
{
	for (int layout = 0; layout < ARENA_LAYOUT_COUNT; ++layout)
	{
		int count;
		const world_rect *b;
		world_point at;

		arena_set_layout(layout);
		b = arena_blocks(&count);
		at = arena_start();
		for (int i = 0; i < count; ++i)
			CHECK(!inside_grown(at, &b[i], 1000));
		CHECK_EQ(2000, collision_arena(at, arena_start_heading(), 2000, 0, 0));
	}
	arena_set_layout(ARENA_OPEN);
}

static void a_spot_in_a_block_is_moved_out(void)
{
	world_point p = {500, 2800}; // in the donut's block, near its south side (+y)
	world_point q;

	arena_set_layout(ARENA_DONUT);
	q = arena_clear_spot(p, 500);
	CHECK_EQ(500, q.x); // out the nearest side: straight south
	CHECK_EQ(3000 + ARENA_WALL_CLEARANCE + 500, q.y);
	// Outside the walls: brought in
	p.x = 9000;
	p.y = 6000;
	q = arena_clear_spot(p, 500);
	CHECK_EQ(arena_player_limit() - 500, q.x);
	CHECK_EQ(6000, q.y);
	arena_set_layout(ARENA_OPEN);
}

// The walls pulled in by any number of units; their columns stay on the grid from the middle
static void inset_pulls_the_walls_in_smoothly(void)
{
	cycle c;

	CHECK_EQ(0, arena_inset());
	CHECK_EQ(37, arena_set_inset(37));
	CHECK_EQ(8000 - 37, arena_half());
	CHECK_EQ(8000 - 37 - ARENA_WALL_CLEARANCE, arena_player_limit());
	CHECK_EQ(8000, arena_grid_half()); // the first column at or past the wall: 7963 rounds up
	arena_set_inset(260);
	CHECK_EQ(7750, arena_grid_half()); // 7740 up to the next 250
	CHECK_EQ(62, arena_grid_pieces());
	// Never past ARENA_INSET_LEAVES from the middle
	CHECK_EQ(8000 - ARENA_INSET_LEAVES, arena_set_inset(100000));
	CHECK_EQ(ARENA_INSET_LEAVES, arena_half());
	CHECK_EQ(0, arena_set_inset(-5));
	CHECK_EQ(8000, arena_half());
	// A wall coming in over a cycle: pulled inside, and that's a crash
	cycle_init(&c, 7800, 0, HEADING_NORTH);
	arena_set_inset(500);
	CHECK(cycle_update(&c, 0, 0, 0));
	CHECK_EQ(arena_player_limit(), c.position.x);
	arena_set_inset(0);
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
	RUN(each_layout_has_its_blocks);
	RUN(blocks_are_whole_pieces_at_every_size);
	RUN(rooms_are_closed_but_for_the_doors);
	RUN(every_start_is_clear_with_room_ahead);
	RUN(a_spot_in_a_block_is_moved_out);
	RUN(inset_pulls_the_walls_in_smoothly);
}
