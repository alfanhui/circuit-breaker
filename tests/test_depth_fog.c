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

void run_depth_fog_tests(void)
{
	RUN(agrees_with_the_floor_at_every_distance);
	RUN(clear_near_and_full_far);
	RUN(behind_the_camera_is_clear);
	RUN(only_distance_ahead_counts);
	RUN(follows_the_camera);
	RUN(gouraud_runs_from_no_change_to_sixteen_darker);
	RUN(gouraud_darkens_steadily_and_greys);
}
