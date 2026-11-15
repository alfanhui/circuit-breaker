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
	const city_tower *entry = city_tower_at(0, CITY_ENTRY_ROW);

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
	CHECK_EQ(city_tower_at(0, 5)->out_max, city_tower_at(1, 5)->out_min);
	CHECK_EQ(CITY_STREET, city_tower_at(2, 5)->out_min - city_tower_at(1, 5)->out_max);
	CHECK_EQ(CITY_STREET, city_tower_at(0, 6)->across_min - city_tower_at(0, 5)->across_max);
	CHECK_EQ(city_tower_at(0, 6)->across_max, city_tower_at(0, 7)->across_min); // rows 6 and 7 touch
	CHECK_EQ(CITY_HUB, city_tower_at(1, 0)->kind); // the CD block
	CHECK_EQ(CITY_HUB, city_tower_at(3, 11)->kind); // Backup RAM
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

// From the CD block's hub (0010) and the roof beside it (0110), a run jumps back across the street
// to 0020 and 0120: they're level with them (tools/work_ram_plan.py, HEIGHTS), as a jump can't
// land higher than it left. Facing out (+x here)
static void the_hub_jumps_back_across_the_street(void)
{
	static const int rows[2] = {0, 1};
	corridor c;
	walker_area area;
	const walker_controls ahead = {0, 1, 0, 0, 0};

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	for (int k = 0; k < 2; ++k)
	{
		const city_tower *from = city_tower_at(1, rows[k]), *to = city_tower_at(2, rows[k]);
		const int across = (from->across_min + from->across_max) >> 1;
		walker w;
		int frames;

		CHECK_EQ(from->height, to->height);
		walker_init_at(&w, corridor_point(&c, from->out_min + 30, across), from->height, 0xC000);
		for (frames = 0; frames < 400 && !w.falling; ++frames)
			walker_update(&w, ahead, &area);
		CHECK(w.falling);
		for (frames = 0; frames < 200 && w.falling; ++frames)
			walker_update(&w, ahead, &area);
		CHECK(!w.falling);
		CHECK_EQ(to->height, walker_height(&w));
		CHECK(corridor_out(&c, walker_position(&w)) > to->out_min); // across the street
	}
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

// Walking (not running: from 30 off, too near to get up to WALKER_MOVE_SPEED) to the street's edge,
// you stop at its lip, and pushing on never takes you over: there's nothing below a street to land on
static void a_street_never_lets_you_fall(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init(&w, corridor_point(&c, CORRIDOR_LENGTH + 250, 470), 0x8000);
	run(&w, ahead, &area, 600);
	CHECK(!w.falling);
	CHECK_EQ(0, walker_height(&w));
	CHECK(walker_position(&w).y <= 500);
}

/*
 * The way to the CD block (tools/work_ram_plan.py's route, 28 moves): every step is a move the
 * walker can make in the table's towers. Side by side in a block (they touch): down at most a clean
 * drop (120), or up at most a climb (100). Across a street (200, a jump: walker.h clears ~225): at
 * most a clean drop down, and back up no more than a caught ledge reaches (STREET_UP_MAX: walker.c's
 * catch_ledge, from a jump at a run's start). Never onto a guard page or a sealed roof
 */
#define GAP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define STREET_UP_MAX 60
static const signed char cd_route[][2] = {
	{0, 5}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {2, 9}, {2, 10}, {2, 11}, {3, 11}, {4, 11}, {4, 10}, {4, 9}, {4, 8}, {5, 8},
	{6, 8}, {6, 7}, {6, 6}, {5, 6}, {4, 6}, {3, 6}, {3, 5}, {3, 4}, {3, 3}, {3, 2}, {3, 1}, {3, 0}, {2, 0}, {1, 0}};
#define CD_ROUTE_STEPS ((int)(sizeof(cd_route) / sizeof(cd_route[0])))

static void the_way_to_the_cd_block_is_walkable(void)
{
	const signed char(*route)[2] = cd_route;
	const int steps = CD_ROUTE_STEPS;

	CHECK_EQ(29, steps);
	CHECK_EQ(CITY_HUB, city_tower_at(route[steps - 1][0], route[steps - 1][1])->kind);
	for (int i = 1; i < steps; ++i)
	{
		const city_tower *a = city_tower_at(route[i - 1][0], route[i - 1][1]);
		const city_tower *b = city_tower_at(route[i][0], route[i][1]);
		// The gap between them, along whichever way they're side by side (the other overlaps)
		// Each way: how far apart their facing edges are (0: they touch; negative: they overlap)
		const int gap_out = GAP_MAX(b->out_min - a->out_max, a->out_min - b->out_max);
		const int gap_across = GAP_MAX(b->across_min - a->across_max, a->across_min - b->across_max);
		const int gap = gap_out > gap_across ? gap_out : gap_across;
		const int up = b->height - a->height;

		CHECK(b->kind == CITY_ROOF || b->kind == CITY_HUB);
		CHECK(gap == 0 || gap == CITY_STREET);
		if (gap == 0)
			CHECK(up >= -120 && up <= WALKER_CLIMB);
		else
			CHECK(up <= STREET_UP_MAX && up >= -120);
		// And back: the way home from the CD block is the same towers the other way
		if (gap == 0)
			CHECK(-up >= -120 && -up <= WALKER_CLIMB);
		else
			CHECK(-up <= STREET_UP_MAX && -up >= -120);
	}
}

/*
 * One move of the route, played: stood on tower a's far side (30 in from the edge away from b, in
 * the middle the other way), facing b, forward held until you're stood on b's roof. Whether you got
 * there, cleanly (no stagger), within 600 frames. The way's along x or y: the towers line up
 */
static bool run_across(const corridor *c, const walker_area *area, const city_tower *a, const city_tower *b)
{
	const walker_controls ahead = {0, 1, 0, 0, 0};
	world_point am, an, bm, bn;
	int ax, ay, bx, by, x, y, yaw;
	walker w;

	city_tower_corners(c, a, &am, &an);
	city_tower_corners(c, b, &bm, &bn);
	ax = (am.x + an.x) / 2, ay = (am.y + an.y) / 2;
	bx = (bm.x + bn.x) / 2, by = (bm.y + bn.y) / 2;
	x = ax, y = ay;
	// The way to b, and its yaw (north, -y, is 0; a quarter turn is west, +x... as the tests above:
	// 0xC000 faces +x, 0x4000 -x, 0x8000 +y)
	if (bx != ax)
	{
		yaw = bx > ax ? 0xC000 : 0x4000;
		x = bx > ax ? ax - CITY_TOWER_SIZE / 2 + 30 : ax + CITY_TOWER_SIZE / 2 - 30;
	}
	else
	{
		yaw = by > ay ? 0x8000 : 0;
		y = by > ay ? ay - CITY_TOWER_SIZE / 2 + 30 : ay + CITY_TOWER_SIZE / 2 - 30;
	}
	{
		world_point at = {(short)x, (short)y};

		walker_init_at(&w, at, a->height, yaw);
	}
	for (int frames = 0; frames < 600; ++frames)
	{
		walker_update(&w, ahead, area);
		if (!w.falling && walker_height(&w) == b->height && w.stagger == 0)
		{
			const world_point p = walker_position(&w);
			const int lo_x = bm.x < bn.x ? bm.x : bn.x, hi_x = bm.x < bn.x ? bn.x : bm.x;
			const int lo_y = bm.y < bn.y ? bm.y : bn.y, hi_y = bm.y < bn.y ? bn.y : bm.y;

			if (p.x >= lo_x && p.x <= hi_x && p.y >= lo_y && p.y <= hi_y)
				return true;
		}
		if (w.stagger > 0)
			return false;
	}
	return false;
}

// The whole way played (the user, 2026-11-14: you couldn't jump back from 0130 to 0230 coming home
// from the CD block): from the entry down to the CD block, then back from it to Backup RAM's hub,
// each move run as you'd run it. The streets on the way back are jumps up, onto ledges you catch
static void the_way_there_and_back_is_runnable(void)
{
	corridor c;
	walker_area area;
	int backup = -1;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	for (int i = 1; i < CD_ROUTE_STEPS; ++i)
	{
		const bool ok = run_across(&c, &area, city_tower_at(cd_route[i - 1][0], cd_route[i - 1][1]),
								   city_tower_at(cd_route[i][0], cd_route[i][1]));

		if (!ok)
			printf("    there: %02d%d0 -> %02d%d0 failed\n", cd_route[i - 1][1], cd_route[i - 1][0], cd_route[i][1], cd_route[i][0]);
		CHECK(ok);
		if (city_tower_at(cd_route[i][0], cd_route[i][1])->kind == CITY_HUB && backup < 0)
			backup = i; // Backup RAM's, on the way
	}
	CHECK(backup > 0);
	for (int i = CD_ROUTE_STEPS - 1; i > backup; --i)
	{
		const bool ok = run_across(&c, &area, city_tower_at(cd_route[i][0], cd_route[i][1]),
								   city_tower_at(cd_route[i - 1][0], cd_route[i - 1][1]));

		if (!ok)
			printf("    back: %02d%d0 -> %02d%d0 failed\n", cd_route[i][1], cd_route[i][0], cd_route[i - 1][1], cd_route[i - 1][0]);
		CHECK(ok);
	}
}

// In the new part, a street jumped as the walker does it: on (2, 9), -310, running south (across +:
// a half turn) off its edge, over the street onto (2, 10), 50 lower: clean
static void a_run_jumps_onto_backup_rams_block(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	const city_tower *from = city_tower_at(2, 9);
	const int middle_out = (from->out_min + from->out_max) >> 1;
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init_at(&w, corridor_point(&c, middle_out, from->across_min + 20), from->height, 0x8000);
	for (frames = 0; frames < 400 && !w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK(w.falling);
	for (frames = 0; frames < 200 && w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK(!w.falling);
	CHECK_EQ(city_tower_at(2, 10)->height, walker_height(&w));
	CHECK_EQ(0, w.stagger);
}

// The middle of a tower's roof in the world, and an eye stood on it
static void eye_on(const corridor *c, int column, int row, int *x, int *y, int *z)
{
	const city_tower *t = city_tower_at(column, row);
	world_point a, b;

	city_tower_corners(c, t, &a, &b);
	*x = (a.x + b.x) / 2;
	*y = (a.y + b.y) / 2;
	*z = t->height + WALKER_EYE;
}

// Whether the drone, stood at stop `stop` of its route, catches the eye as the sheet sweeps down
// its floor
static bool caught_at_stop(const corridor *c, int stop, int x, int y, int z)
{
	static drone_stop route[CITY_DRONE_STOPS];
	static drone_box boxes[CITY_TOWERS];
	drone d;

	city_drone_route(c, route);
	city_drone_boxes(c, boxes);
	drone_init(&d, &route[stop], 1); // a route of that one stop: it stays there
	for (int i = 0; i < DRONE_SCAN_FRAMES; ++i)
	{
		if (drone_sees(&d, x, y, z, boxes, CITY_TOWERS))
			return true;
		drone_update(&d);
	}
	return false;
}

// Its stops are in the street, out of every tower, each lower than the last on the way down
static void the_drone_rides_the_street(void)
{
	corridor c;
	drone_stop route[CITY_DRONE_STOPS];
	drone_box boxes[CITY_TOWERS];

	open_round_five(&c);
	city_drone_route(&c, route);
	city_drone_boxes(&c, boxes);
	for (int s = 0; s < CITY_DRONE_STOPS; ++s)
		for (int i = 0; i < CITY_TOWERS; ++i)
			CHECK(route[s].x < boxes[i].x_min || route[s].x > boxes[i].x_max || route[s].y < boxes[i].y_min ||
				  route[s].y > boxes[i].y_max || route[s].z > boxes[i].top);
	CHECK(route[1].z < route[0].z);
	CHECK(route[2].z < route[1].z);
	CHECK_EQ(route[1].z, route[3].z);
}

// The way down: each stop's scan sweeps the roofs of column 0 beside it (column 1's are under its
// nose: inside the fan's 45 degrees only close to its middle line), and not another stop's
static void the_drone_watches_the_way_down(void)
{
	corridor c;
	int x, y, z;

	open_round_five(&c);
	eye_on(&c, 0, 7, &x, &y, &z);
	CHECK(caught_at_stop(&c, 0, x, y, z));
	CHECK(!caught_at_stop(&c, 2, x, y, z)); // the bottom stop's fan doesn't reach so far aside
	eye_on(&c, 1, 7, &x, &y, &z);
	CHECK(!caught_at_stop(&c, 0, x, y, z)); // under its nose, 450 aside: out of the fan
	eye_on(&c, 0, 9, &x, &y, &z);
	CHECK(caught_at_stop(&c, 1, x, y, z));
	eye_on(&c, 0, 11, &x, &y, &z);
	CHECK(caught_at_stop(&c, 2, x, y, z));
	CHECK(!caught_at_stop(&c, 0, x, y, z));
}

void run_city_tests(void)
{
	RUN(the_drone_rides_the_street);
	RUN(the_drone_watches_the_way_down);
	RUN(the_way_to_the_cd_block_is_walkable);
	RUN(the_way_there_and_back_is_runnable);
	RUN(a_run_jumps_onto_backup_rams_block);
	RUN(the_towers_are_on_the_plans_grid);
	RUN(the_corridor_has_room_for_the_city);
	RUN(out_of_the_corridor_onto_the_entry);
	RUN(a_run_jumps_the_first_street);
	RUN(the_hub_jumps_back_across_the_street);
	RUN(a_street_never_lets_you_fall);
	RUN(a_drop_off_the_entry_climbs_back);
}
