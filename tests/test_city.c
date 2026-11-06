#include "test.h"
#include "../src/game/city.h"

#define HALF 8000

// Round 5's corridor: behind the west wall (+x, out), -500 to 500 along y (across), as
// test_corridor.c's
static void open_round_five(corridor *c)
{
	wall_flicker f;

	wall_flicker_set_round(&f, 4);
	corridor_open(c, &f, HALF);
}

static void run(walker *w, walker_controls c, const walker_area *area, int frames)
{
	for (int i = 0; i < frames; ++i)
		walker_update(w, c, area);
}

// The plan's shape: 500 towers, touching in blocks of two, a street (200) after every second one;
// the entry right at the corridor's far end, filling its right half, level with its floor
static void the_towers_are_on_the_plans_grid(void)
{
	const city_tower *entry = &city_towers[6]; // row 5, column 0

	for (int i = 0; i < CITY_TOWERS; ++i)
	{
		CHECK_EQ(CITY_TOWER_SIZE, city_towers[i].out_max - city_towers[i].out_min);
		CHECK_EQ(CITY_TOWER_SIZE, city_towers[i].across_max - city_towers[i].across_min);
		CHECK(city_towers[i].height > CITY_GROUND);
	}
	CHECK_EQ(CORRIDOR_LENGTH, entry->out_min);
	CHECK_EQ(0, entry->across_min);
	CHECK_EQ(0, entry->height);
	// Columns 0 and 1 touch; a street before column 2. Rows 5 and 6: a street
	CHECK_EQ(city_towers[6].out_max, city_towers[7].out_min);
	CHECK_EQ(CITY_STREET, city_towers[8].out_min - city_towers[7].out_max);
	CHECK_EQ(CITY_STREET, city_towers[9].across_min - city_towers[6].across_max);
	CHECK_EQ(city_towers[9].across_max, city_towers[12].across_min); // rows 6 and 7 touch
}

// The corridor's area has room for the city: its floor and walls (5), the grey box (25), the towers
static void the_corridor_has_room_for_the_city(void)
{
	corridor c;
	walker_area area;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	CHECK_EQ(5 + CORRIDOR_BOXES + CITY_TOWERS, area.count);
}

// Out of the corridor's right half, past the far end (open there now), onto the entry's roof at
// the floor's height, and on off its far side: down 50 onto the next tower, cleanly. Facing out is
// west here (+x: a three-quarter turn)
static void out_of_the_corridor_onto_the_entry(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init(&w, corridor_point(&c, 4300, 250), 0xC000);
	for (frames = 0; frames < 600 && !w.falling; ++frames)
	{
		walker_update(&w, ahead, &area);
		if (corridor_out(&c, walker_position(&w)) > CORRIDOR_LENGTH && !w.falling)
			CHECK_EQ(0, walker_height(&w)); // on the entry: no step, no lip at the corridor's end
	}
	CHECK(w.falling); // off the entry's far side
	CHECK(corridor_out(&c, walker_position(&w)) >= CORRIDOR_LENGTH + CITY_TOWER_SIZE - WALKER_FOOT);
	run(&w, ahead, &area, 200);
	CHECK(!w.falling);
	CHECK_EQ(-50, walker_height(&w));
	CHECK_EQ(0, w.stagger);
}

// A run across the entry's roof jumps the street at its south side (across +: +y, a half turn)
// onto the next block's tower, 60 down: clean. From 60 across: clear of the entry's north edge
static void a_run_jumps_the_first_street(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init(&w, corridor_point(&c, CORRIDOR_LENGTH + 250, 60), 0x8000);
	for (frames = 0; frames < 400 && !w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK(w.falling);
	CHECK_EQ(WALKER_RUN_SPEED, w.forward); // a jump, not a step off: you never stopped
	for (frames = 0; frames < 200 && w.falling; ++frames) // to the landing (held on, you run on)
		walker_update(&w, ahead, &area);
	CHECK(!w.falling);
	CHECK_EQ(-60, walker_height(&w));
	CHECK_EQ(0, w.stagger);
	CHECK(walker_position(&w).y >= 500 + CITY_STREET); // across the street
}

// Dropped off the entry onto the tower beside it (50 down), you can run back and climb up onto
// the entry again: facing back towards the corridor (east, -x here: a quarter turn)
static void a_drop_off_the_entry_climbs_back(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init_at(&w, corridor_point(&c, CORRIDOR_LENGTH + CITY_TOWER_SIZE + 450, 250), -50, 0x4000);
	for (frames = 0; frames < 400 && walker_height(&w) == -50; ++frames)
		walker_update(&w, ahead, &area);
	CHECK_EQ(0, walker_height(&w)); // up on the entry
	CHECK(corridor_out(&c, walker_position(&w)) <= CORRIDOR_LENGTH + CITY_TOWER_SIZE);
}

// Walking (not running) to the street's edge, you stop at its lip, and pushing on never takes you
// over: there's nothing below a street to land on
static void a_street_never_lets_you_fall(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init(&w, corridor_point(&c, CORRIDOR_LENGTH + 250, 400), 0x8000);
	run(&w, ahead, &area, 600);
	CHECK(!w.falling);
	CHECK_EQ(0, walker_height(&w));
	CHECK(walker_position(&w).y <= 500);
}

void run_city_tests(void)
{
	RUN(the_towers_are_on_the_plans_grid);
	RUN(the_corridor_has_room_for_the_city);
	RUN(out_of_the_corridor_onto_the_entry);
	RUN(a_run_jumps_the_first_street);
	RUN(a_street_never_lets_you_fall);
	RUN(a_drop_off_the_entry_climbs_back);
}
