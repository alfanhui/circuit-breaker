#include "test.h"
#include "../src/game/depth_fog.h"

#define START 400 // the 1st person floor fade's figures (floor_fade_vdp2.c)
#define END 3000

// The floor's own rule for the level at a distance (floor_fade.c)
static int floor_level(int distance)
{
	if (distance <= START)
		return 0;
	if (distance >= END)
		return DEPTH_FOG_LEVELS - 1;
	return (distance - START) * (DEPTH_FOG_LEVELS - 1) / (END - START);
}

// A camera at the origin facing north (-y)
static depth_fog facing_north(void)
{
	depth_fog f;
	world_point eye = {0, 0};

	depth_fog_set_distances(&f, START, END);
	depth_fog_set_camera(&f, eye, 0, -1024);
	return f;
}

static void agrees_with_the_floor_at_every_distance(void)
{
	depth_fog f = facing_north();
	int mismatches = 0;

	// A wall must fade exactly as much as the floor it stands on
	for (int d = 0; d <= END + 500; ++d)
		if (depth_fog_level(&f, 0, -d) != floor_level(d))
			++mismatches;
	CHECK_EQ(0, mismatches);
}

static void clear_near_and_full_far(void)
{
	depth_fog f = facing_north();

	CHECK_EQ(0, depth_fog_level(&f, 0, -START));
	CHECK_EQ(DEPTH_FOG_LEVELS - 1, depth_fog_level(&f, 0, -END));
	CHECK_EQ(DEPTH_FOG_LEVELS - 1, depth_fog_level(&f, 0, -28000)); // across the largest arena
}

static void behind_the_camera_is_clear(void)
{
	depth_fog f = facing_north();

	CHECK_EQ(0, depth_fog_level(&f, 0, 2000));
}

static void only_distance_ahead_counts(void)
{
	depth_fog f = facing_north();

	// Off to the side at the same distance ahead: the same floor row, so the same level
	CHECK_EQ(depth_fog_level(&f, 0, -1500), depth_fog_level(&f, 1200, -1500));
	CHECK_EQ(depth_fog_level(&f, 0, -1500), depth_fog_level(&f, -1200, -1500));
}

static void follows_the_camera(void)
{
	depth_fog f;
	world_point eye = {5000, -3000};

	// Facing east (-x) from somewhere else in the arena
	depth_fog_set_distances(&f, START, END);
	depth_fog_set_camera(&f, eye, -1024, 0);
	CHECK_EQ(0, depth_fog_level(&f, 5000 - START, -3000));
	CHECK_EQ(floor_level(1700), depth_fog_level(&f, 5000 - 1700, -3000));
	CHECK_EQ(DEPTH_FOG_LEVELS - 1, depth_fog_level(&f, 5000 - END, -3000));
}

static void gouraud_runs_from_no_change_to_sixteen_darker(void)
{
	CHECK_EQ(DEPTH_FOG_GOURAUD_NEUTRAL, depth_fog_gouraud_colour(0));
	CHECK_EQ(0x8000, depth_fog_gouraud_colour(DEPTH_FOG_LEVELS - 1)); // every channel 0: 16 darker
	CHECK_EQ(DEPTH_FOG_GOURAUD_NEUTRAL, depth_fog_gouraud_colour(-3));		   // clamped
	CHECK_EQ(0x8000, depth_fog_gouraud_colour(DEPTH_FOG_LEVELS + 5));
}

static void gouraud_darkens_steadily_and_greys(void)
{
	int last = 17;

	for (int level = 0; level < DEPTH_FOG_LEVELS; ++level)
	{
		unsigned short c = depth_fog_gouraud_colour(level);
		int r = c & 0x1F;
		CHECK_EQ(r, (c >> 5) & 0x1F); // the same on every channel: darker, not tinted
		CHECK_EQ(r, (c >> 10) & 0x1F);
		CHECK(r < last); // every level darker than the one before
		last = r;
	}
}

// A flat colour darkened as Gouraud would: each channel down, none below black, bit 15 kept
static void darken_takes_steps_off_each_channel(void)
{
	CHECK_EQ(0x8000 | 10 << 10 | 20 << 5 | 30, depth_fog_darken(0x8000 | 10 << 10 | 20 << 5 | 30, 0));
	CHECK_EQ(0x8000 | 6 << 10 | 16 << 5 | 26, depth_fog_darken(0x8000 | 10 << 10 | 20 << 5 | 30, 4));
	CHECK_EQ(0x8000 | 0 << 10 | 4 << 5 | 14, depth_fog_darken(0x8000 | 10 << 10 | 20 << 5 | 30, 16));
	// The most a level takes off is the Gouraud's: 16 - its channel
	CHECK_EQ(0x8000 | 0 << 10 | 4 << 5 | 14,
			 depth_fog_darken(0x8000 | 10 << 10 | 20 << 5 | 30, 16 - (depth_fog_gouraud_colour(DEPTH_FOG_LEVELS - 1) & 31)));
}

// A trail piece (a a b b) and an arena strip (a b b a) each find a shared table, and it holds
// their corners; the same two levels in the two orders are different tables
static void two_level_pieces_share_a_table(void)
{
	int levels[4];
	const int trail = depth_fog_shared_table(3, 3, 9, 9);
	const int strip = depth_fog_shared_table(3, 9, 9, 3);

	CHECK(trail >= 0 && trail < DEPTH_FOG_SHARED_TABLES);
	CHECK(strip >= 0 && strip < DEPTH_FOG_SHARED_TABLES);
	CHECK(trail != strip);
	depth_fog_shared_levels(trail, levels);
	CHECK_EQ(3, levels[0]);
	CHECK_EQ(3, levels[1]);
	CHECK_EQ(9, levels[2]);
	CHECK_EQ(9, levels[3]);
	depth_fog_shared_levels(strip, levels);
	CHECK_EQ(3, levels[0]);
	CHECK_EQ(9, levels[1]);
	CHECK_EQ(9, levels[2]);
	CHECK_EQ(3, levels[3]);
	// One level all round: a shared table too
	CHECK(depth_fog_shared_table(15, 15, 15, 15) >= 0);
}

// Every shared table's corners come back to it: all 512 are reachable and none is used twice
static void every_shared_table_round_trips(void)
{
	for (int t = 0; t < DEPTH_FOG_SHARED_TABLES; ++t)
	{
		int levels[4];

		depth_fog_shared_levels(t, levels);
		for (int k = 0; k < 4; ++k)
			CHECK(levels[k] >= 0 && levels[k] < DEPTH_FOG_LEVELS);
		// a a a a is both orders: it's found as a a b b, the first
		if (levels[0] == levels[1] && levels[1] == levels[2] && t >= DEPTH_FOG_LEVELS * DEPTH_FOG_LEVELS)
			continue;
		CHECK_EQ(t, depth_fog_shared_table(levels[0], levels[1], levels[2], levels[3]));
	}
}

// A face cut by the view (on foot) can have any four levels: no shared table, its own is written
static void other_corners_have_none(void)
{
	CHECK_EQ(-1, depth_fog_shared_table(1, 2, 3, 4));
	CHECK_EQ(-1, depth_fog_shared_table(1, 2, 1, 2));
	CHECK_EQ(-1, depth_fog_shared_table(5, 5, 5, 6));
}

// A box is wholly in full fog exactly when all four of its corners are, whichever way you face
// (level.c leaves the city's blocks that are out, under your eye)
static void a_box_is_full_when_every_corner_is(void)
{
	const int forwards[8][2] = {{0, -1024}, {0, 1024}, {1024, 0}, {-1024, 0}, {724, -724}, {-724, 724}, {300, 979}, {-979, -300}};
	unsigned int r = 99;
	int disagree = 0, full = 0;

	for (int k = 0; k < 8; ++k)
	{
		depth_fog f;
		world_point eye = {100, -200};

		depth_fog_set_distances(&f, START, END);
		depth_fog_set_camera(&f, eye, forwards[k][0], forwards[k][1]);
		for (int n = 0; n < 2000; ++n)
		{
			int x, y, w, h, all;

			r = r * 1103515245u + 12345u;
			x = (int)((r >> 8) % 12000u) - 6000;
			r = r * 1103515245u + 12345u;
			y = (int)((r >> 8) % 12000u) - 6000;
			w = 50 + (int)((r >> 4) & 1023);
			h = 50 + (int)((r >> 14) & 1023);
			all = depth_fog_level(&f, x, y) == DEPTH_FOG_LEVELS - 1 && depth_fog_level(&f, x + w, y) == DEPTH_FOG_LEVELS - 1 &&
				  depth_fog_level(&f, x, y + h) == DEPTH_FOG_LEVELS - 1 && depth_fog_level(&f, x + w, y + h) == DEPTH_FOG_LEVELS - 1;
			full += all;
			if (all != depth_fog_box_full(&f, x, x + w, y, y + h))
				++disagree;
		}
	}
	CHECK_EQ(0, disagree);
	CHECK(full > 100); // (and the test met some)
}

void run_depth_fog_tests(void)
{
	RUN(a_box_is_full_when_every_corner_is);
	RUN(two_level_pieces_share_a_table);
	RUN(every_shared_table_round_trips);
	RUN(other_corners_have_none);
	RUN(darken_takes_steps_off_each_channel);
	RUN(agrees_with_the_floor_at_every_distance);
	RUN(clear_near_and_full_far);
	RUN(behind_the_camera_is_clear);
	RUN(only_distance_ahead_counts);
	RUN(follows_the_camera);
	RUN(gouraud_runs_from_no_change_to_sixteen_darker);
	RUN(gouraud_darkens_steadily_and_greys);
}
