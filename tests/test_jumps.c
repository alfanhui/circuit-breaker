#include <string.h>
#include "test.h"
#include "../src/game/jumps.h"
#include "../src/game/city.h"

/*
 * The pause menu's jumps (game/jumps.h): each place stands you on something, at its height, and
 * leaves you there (not falling, nothing pushing you off); "stay" is nowhere
 */

#define HALF 8000

static void open_round_five(corridor *c)
{
	wall_flicker f;

	wall_flicker_set_round(&f, 4);
	corridor_open(c, &f, HALF);
}

static void stay_is_nowhere(void)
{
	corridor c;
	world_point at = {123, 456};
	int height = 7, yaw = 9;

	open_round_five(&c);
	CHECK(!jump_where(&c, JUMP_NONE, &at, &height, &yaw));
	CHECK_EQ(123, at.x); // nothing written
	CHECK_EQ(7, height);
	CHECK(!jump_where(&c, JUMP_COUNT, &at, &height, &yaw));
}

// Stood at each for a second, no pad: still there, on its height, not falling
static void every_place_stands_you_on_something(void)
{
	corridor c;
	walker_area area;
	const walker_controls none = {0, 0, 0, 0, 0};

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	for (int place = JUMP_NONE + 1; place < JUMP_COUNT; ++place)
	{
		world_point at;
		int height, yaw;
		walker w;

		CHECK(jump_where(&c, place, &at, &height, &yaw));
		walker_init_at(&w, at, height, yaw);
		for (int i = 0; i < 60; ++i)
			walker_update(&w, none, &area);
		CHECK(!w.falling);
		CHECK_EQ(height, walker_height(&w));
		CHECK_EQ(at.x, walker_position(&w).x);
		CHECK_EQ(at.y, walker_position(&w).y);
		CHECK(!walker_fell_off(&w, &area));
	}
}

// The corridor's are in it, the city's out past its end; the hubs' on their hubs
static void the_places_are_where_they_say(void)
{
	corridor c;
	world_point at;
	int height, yaw;

	open_round_five(&c);
	jump_where(&c, JUMP_YARD, &at, &height, &yaw);
	CHECK(corridor_out(&c, at) < CORRIDOR_LENGTH);
	jump_where(&c, JUMP_ENTRY, &at, &height, &yaw);
	CHECK(corridor_out(&c, at) > CORRIDOR_LENGTH);
	CHECK_EQ(0, height);
	jump_where(&c, JUMP_CD_HUB, &at, &height, &yaw);
	CHECK(city_on_hub(&c, at, height));
	CHECK_EQ(city_tower_at(CITY_HUB_COLUMN, CITY_HUB_ROW)->height, height);
	jump_where(&c, JUMP_BACKUP, &at, &height, &yaw);
	CHECK(city_on_hub(&c, at, height));
	jump_where(&c, JUMP_SCSP, &at, &height, &yaw);
	CHECK(city_on_hub(&c, at, height));
	// Facing out along the corridor: its side's heading (round 5's: west, +x)
	jump_where(&c, JUMP_START, &at, &height, &yaw);
	CHECK_EQ(c.side * 0x4000, yaw);
}

static void the_names_fit(void)
{
	for (int place = 0; place < JUMP_COUNT; ++place)
	{
		CHECK(strlen(jump_name(place)) > 0);
		CHECK((int)strlen(jump_name(place)) <= JUMP_NAME_MAX);
	}
}

void run_jumps_tests(void)
{
	RUN(stay_is_nowhere);
	RUN(every_place_stands_you_on_something);
	RUN(the_places_are_where_they_say);
	RUN(the_names_fit);
}
