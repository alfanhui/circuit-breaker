#include "test.h"
#include "../src/game/wall_flicker.h"
#include "../src/game/rounds.h"
#include "../src/game/arena.h"

// How many of `steps` steps something glitches
static int glitching_steps(wall_flicker *f, int steps)
{
	int n = 0;

	for (int i = 0; i < steps; ++i)
	{
		wall_flicker_step(f);
		n += (f->hidden | f->flashed) != 0;
	}
	return n;
}

static void round_one_has_none(void)
{
	wall_flicker f;

	wall_flicker_set_round(&f, 0);
	CHECK_EQ(0, f.count);
	CHECK_EQ(0, glitching_steps(&f, 5000));
}

// Every later round has one, somewhere different each time, on the walls' grid, inside the
// square even when round 5 has closed in all the way, and growing
static void each_later_round_has_one_somewhere_else(void)
{
	wall_flicker f;
	int last_count = 0;
	int last_side = -1;
	int last_min = 0;

	for (int round = 1; round < ROUND_COUNT; ++round)
	{
		wall_flicker_set_round(&f, round);
		CHECK(f.count >= 3); // 3-4 pieces: a gap, not a long run
		CHECK(f.count <= WALL_FLICKER_PIECES_MAX);
		CHECK(f.count >= last_count);
		CHECK(f.side != last_side || f.along_min != last_min);
		CHECK_EQ(0, f.along_min % WALL_FLICKER_PIECE);
		CHECK(f.along_min >= -(8000 - ROUND_CLOSE_MAX));
		CHECK(wall_flicker_along_max(&f) <= 8000 - ROUND_CLOSE_MAX);
		last_count = f.count;
		last_side = f.side;
		last_min = f.along_min;
	}
}

// Each is on the far side from where its round starts you: at least the centre-to-wall distance
// away (as far as any wall is from the middle), in the round's own layout
static void far_from_where_you_start(void)
{
	rounds r;

	for (int round = 1; round < ROUND_COUNT; ++round)
	{
		wall_flicker f;
		world_point start;
		int middle, x, y, dx, dy;

		rounds_init(&r);
		r.number = (unsigned char)round;
		arena_set_layout(rounds_layout(&r));
		start = arena_start();
		wall_flicker_set_round(&f, round);
		// The section's middle, on its wall (north is -y, east is -x: world.h)
		middle = (f.along_min + wall_flicker_along_max(&f)) / 2;
		x = f.side == WALL_FLICKER_EAST ? -arena_half() : f.side == WALL_FLICKER_WEST ? arena_half() : middle;
		y = f.side == WALL_FLICKER_NORTH ? -arena_half() : f.side == WALL_FLICKER_SOUTH ? arena_half() : middle;
		dx = x > start.x ? x - start.x : start.x - x;
		dy = y > start.y ? y - start.y : start.y - y;
		CHECK(dx + dy >= arena_half());
	}
	arena_set_layout(ARENA_OPEN);
}

// Rounds 4 and 5: clear of the rooms' cross, which meets every wall at the middle (+-125)
static void clear_of_the_rooms_cross(void)
{
	wall_flicker f;

	wall_flicker_set_round(&f, 3);
	CHECK(f.along_min > 125 || wall_flicker_along_max(&f) < -125);
}

// Only the section's pieces glitch, never hidden and flashed at once, and a burst always shows
static void glitches_stay_in_the_section(void)
{
	wall_flicker f;

	for (int round = 1; round < ROUND_COUNT; ++round)
	{
		wall_flicker_set_round(&f, round);
		for (int i = 0; i < 3000; ++i)
		{
			wall_flicker_step(&f);
			CHECK_EQ(0, (f.hidden | f.flashed) >> f.count);
			CHECK_EQ(0, f.hidden & f.flashed);
		}
	}
}

// It grows: round 5 glitches far more often than round 2, and a burst lasts a few steps
static void it_grows_more_obvious(void)
{
	wall_flicker f;
	int round2, round5;

	wall_flicker_set_round(&f, 1);
	round2 = glitching_steps(&f, 6000);
	wall_flicker_set_round(&f, 4);
	round5 = glitching_steps(&f, 6000);
	CHECK(round2 > 0);
	CHECK(round5 > 4 * round2);
	CHECK(round5 > 6000 / 2); // nearly all the time
	CHECK(round2 < 6000 / 8); // now and then
}

// The same round always glitches the same way (repeatable for tests and E2E shots)
static void the_same_every_time(void)
{
	wall_flicker a, b;

	wall_flicker_set_round(&a, 2);
	wall_flicker_set_round(&b, 2);
	for (int i = 0; i < 1000; ++i)
	{
		wall_flicker_step(&a);
		wall_flicker_step(&b);
		CHECK_EQ(a.hidden, b.hidden);
		CHECK_EQ(a.flashed, b.flashed);
	}
}

// Only round 5's gives way: before it, riding into the flicker is a crash like any other
static void only_the_last_round_gives_way(void)
{
	wall_flicker f;

	for (int round = 0; round < ROUND_COUNT; ++round)
	{
		wall_flicker_set_round(&f, round);
		CHECK_EQ(round == ROUND_COUNT - 1, f.gives_way);
	}
}

// A hit: facing its wall, up against it, along the section (half a piece of forgiveness either
// side). Round 5's is the west wall (+x), -500 to 500 along y
static void a_hit_is_facing_it_against_it_and_along_it(void)
{
	wall_flicker f;
	const int limit = 7900;
	world_point at = {7900, 0};

	wall_flicker_set_round(&f, 4);
	CHECK(wall_flicker_hit(&f, at, HEADING_WEST, limit));
	CHECK(!wall_flicker_hit(&f, at, HEADING_NORTH, limit)); // riding along the wall, not into it
	CHECK(!wall_flicker_hit(&f, at, HEADING_EAST, limit));	// away from it
	at.x = 7899;
	CHECK(!wall_flicker_hit(&f, at, HEADING_WEST, limit)); // stopped short: a trail
	at.x = 7900;
	at.y = 500 + WALL_FLICKER_HIT_MARGIN;
	CHECK(wall_flicker_hit(&f, at, HEADING_WEST, limit)); // just clipping its end counts
	at.y = 500 + WALL_FLICKER_HIT_MARGIN + 1;
	CHECK(!wall_flicker_hit(&f, at, HEADING_WEST, limit));
	at.y = -500 - WALL_FLICKER_HIT_MARGIN;
	CHECK(wall_flicker_hit(&f, at, HEADING_WEST, limit));
	at.y = -500 - WALL_FLICKER_HIT_MARGIN - 1;
	CHECK(!wall_flicker_hit(&f, at, HEADING_WEST, limit));
}

// Each side: north is -y, east -x (world.h); north and south run along x
static void every_side_is_the_right_wall(void)
{
	wall_flicker f;
	const int limit = 5000;
	world_point north = {-2500, -5000};
	world_point east = {-5000, -2000};

	wall_flicker_set_round(&f, 3); // the north wall, -3000 to -2000 along x
	CHECK(wall_flicker_hit(&f, north, HEADING_NORTH, limit));
	north.y = 5000; // the south wall, at the same x
	CHECK(!wall_flicker_hit(&f, north, HEADING_SOUTH, limit));
	wall_flicker_set_round(&f, 2); // the east wall, -2250 to -1500 along y
	CHECK(wall_flicker_hit(&f, east, HEADING_EAST, limit));
	east.x = 5000;
	CHECK(!wall_flicker_hit(&f, east, HEADING_WEST, limit));
}

static void round_one_is_never_hit(void)
{
	wall_flicker f;
	world_point at = {0, -5000};

	wall_flicker_set_round(&f, 0);
	CHECK(!wall_flicker_hit(&f, at, HEADING_NORTH, 5000));
}

// Opened: every piece hidden, none flashed: a hole
static void opened_it_is_a_hole(void)
{
	wall_flicker f;

	wall_flicker_set_round(&f, 4);
	wall_flicker_open(&f);
	CHECK_EQ((1 << f.count) - 1, f.hidden);
	CHECK_EQ(0, f.flashed);
}

void run_wall_flicker_tests(void)
{
	RUN(round_one_has_none);
	RUN(each_later_round_has_one_somewhere_else);
	RUN(far_from_where_you_start);
	RUN(clear_of_the_rooms_cross);
	RUN(glitches_stay_in_the_section);
	RUN(it_grows_more_obvious);
	RUN(the_same_every_time);
	RUN(only_the_last_round_gives_way);
	RUN(a_hit_is_facing_it_against_it_and_along_it);
	RUN(every_side_is_the_right_wall);
	RUN(round_one_is_never_hit);
	RUN(opened_it_is_a_hole);
}
