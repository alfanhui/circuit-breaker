#include "test.h"
#include "../src/game/city.h"

/*
 * Low Work RAM's city (game/city.h): the table is the plan's (tools/ram_banks_plan.py), and here the
 * walker plays it, and the watchdogs' patrols are checked to fly clear of it
 */

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

// How far apart two towers' facing edges are, whichever way they're side by side (0: they touch)
#define GAP_MAX(a, b) ((a) > (b) ? (a) : (b))
static int gap_between(const city_tower *a, const city_tower *b)
{
	const int gap_out = GAP_MAX(b->out_min - a->out_max, a->out_min - b->out_max);
	const int gap_across = GAP_MAX(b->across_min - a->across_max, a->across_min - b->across_max);

	return gap_out > gap_across ? gap_out : gap_across;
}

// The plan's shape: 500 towers, touching in blocks of two, a street (200) after every second one;
// the entry right at the corridor's far end, filling its right half, level with its floor; the
// ways out where the plan put them, by their addresses
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
	// Columns 0 and 1 touch; a street before column 2. Rows 7 and 8: a street; 8 and 9 touch
	CHECK_EQ(city_tower_at(0, 8)->out_max, city_tower_at(1, 8)->out_min);
	CHECK_EQ(CITY_STREET, city_tower_at(2, 8)->out_min - city_tower_at(1, 8)->out_max);
	CHECK_EQ(CITY_STREET, city_tower_at(0, 8)->across_min - city_tower_at(0, 7)->across_max);
	CHECK_EQ(city_tower_at(0, 8)->across_max, city_tower_at(0, 9)->across_min);
	CHECK_EQ(CITY_HUB, city_tower_at(CITY_HUB_COLUMN, CITY_HUB_ROW)->kind);		// the CD block
	CHECK_EQ(CITY_HUB, city_tower_at(CITY_BACKUP_COLUMN, CITY_BACKUP_ROW)->kind); // Backup RAM
	CHECK_EQ(CITY_HUB, city_tower_at(14, 14)->kind);								// the SCSP
	// Pages: 64 KB a row, 4 KB a tower along it, from Low Work RAM's start
	CHECK_EQ(0x00200000u, city_tower_address(0, 0));
	CHECK_EQ(0x002EA000u, city_tower_address(CITY_HUB_COLUMN, CITY_HUB_ROW));
	CHECK_EQ(0x002FF000u, city_tower_address(15, 15));
}

// Every move works both ways (the plan's rule): across every street the two roofs are level (a
// jump either way), and within a block two roofs side by side are no further apart than a climb
// (so it's a clean drop down and a climb back up)
static void every_move_works_both_ways(void)
{
	int streets = 0, steps = 0;

	for (int r = 0; r < CITY_ROWS; ++r)
		for (int c = 0; c < CITY_COLUMNS; ++c)
		{
			const city_tower *a = city_tower_at(c, r);

			for (int way = 0; way < 2; ++way)
			{
				const int c2 = c + (way == 0), r2 = r + (way == 1);
				const city_tower *b;

				if (c2 >= CITY_COLUMNS || r2 >= CITY_ROWS)
					continue;
				b = city_tower_at(c2, r2);
				if (a->kind == CITY_GUARD || b->kind == CITY_GUARD)
					continue;
				if (gap_between(a, b) == CITY_STREET)
				{
					CHECK_EQ(a->height, b->height);
					++streets;
				}
				else
				{
					CHECK(a->height - b->height <= WALKER_CLIMB && b->height - a->height <= WALKER_CLIMB);
					++steps;
				}
			}
		}
	CHECK(streets > 50 && steps > 50); // (it looked at a good many)
}

// The corridor's area has room for the city: its floor and walls (5), the grey box, the towers
static void the_corridor_has_room_for_the_city(void)
{
	corridor c;
	walker_area area;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	CHECK_EQ(5 + CORRIDOR_BOXES + CITY_TOWERS, area.count);
}

// Out of the corridor's right half, past the far end (open there now), onto the entry's roof at
// the floor's height. Straight on is a guard page (the plan: the way's south): you stop at its foot,
// on the entry. Facing out is west here (+x: a three-quarter turn)
static void out_of_the_corridor_onto_the_entry(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init(&w, corridor_point(&c, 4300, 250), 0xC000);
	run(&w, ahead, &area, 400);
	CHECK(!w.falling);
	CHECK_EQ(0, walker_height(&w));
	CHECK(corridor_out(&c, walker_position(&w)) > CORRIDOR_LENGTH);					 // out on the entry
	CHECK(corridor_out(&c, walker_position(&w)) < CORRIDOR_LENGTH + CITY_TOWER_SIZE); // not in the wall
}

// Off the entry's south side (across +: +y, a half turn): within its block, a step of 90 down
// onto (0, 9): a clean drop. (At a run by the edge, so it's a jump down: up first, then the landing)
static void off_the_entry_a_clean_drop(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init(&w, corridor_point(&c, CORRIDOR_LENGTH + 250, 250), 0x8000);
	for (frames = 0; frames < 600 && (w.falling || walker_height(&w) >= 0); ++frames)
		walker_update(&w, ahead, &area);
	CHECK(!w.falling);
	CHECK_EQ(city_tower_at(0, 9)->height, walker_height(&w));
	CHECK_EQ(-90, walker_height(&w));
	CHECK_EQ(0, w.stagger);
}

// On a hub's roof you're safe from the watchdogs: stood on the CD block's hub, yes; just off its
// edge on the next roof, or the entry, no; over it but up in the lift (past CITY_SEEN_EYE), no (the
// lift itself is safe, as input.c says)
static void a_hub_s_roof_is_safe(void)
{
	corridor c;
	const city_tower *hub = city_tower_at(CITY_HUB_COLUMN, CITY_HUB_ROW);
	const city_tower *next = city_tower_at(CITY_HUB_COLUMN - 1, CITY_HUB_ROW);
	const city_tower *entry = city_tower_at(0, CITY_ENTRY_ROW);

	open_round_five(&c);
	CHECK(city_on_hub(&c, corridor_point(&c, hub->out_min + 250, hub->across_min + 250), hub->height));
	CHECK(city_on_hub(&c, corridor_point(&c, hub->out_min + 5, hub->across_min + 5), hub->height));
	CHECK(!city_on_hub(&c, corridor_point(&c, next->out_max - 5, next->across_min + 250), next->height));
	CHECK(!city_on_hub(&c, corridor_point(&c, entry->out_min + 250, entry->across_min + 250), entry->height));
	CHECK(!city_on_hub(&c, corridor_point(&c, hub->out_min + 250, hub->across_min + 250), hub->height + 1000));
}

// The same drop right by the yard's wall (50 out of the corridor, where its side wall used to go on
// 100 past its end, unseen): nothing stops you. (At 40 your body, WALKER_RADIUS, touches the wall's
// end: it stops you, as it should)
static void off_the_entry_by_the_yard_wall(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init(&w, corridor_point(&c, CORRIDOR_LENGTH + 50, 250), 0x8000);
	for (frames = 0; frames < 600 && (w.falling || walker_height(&w) >= 0); ++frames)
		walker_update(&w, ahead, &area);
	CHECK(!w.falling);
	CHECK_EQ(city_tower_at(0, 9)->height, walker_height(&w));
}

// And back: from (0, 9)'s far side, a run north (-y: yaw 0) climbs back up onto the entry
static void a_drop_off_the_entry_climbs_back(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	const city_tower *below = city_tower_at(0, 9);
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init_at(&w, corridor_point(&c, CORRIDOR_LENGTH + 250, below->across_max - 30), below->height, 0);
	for (frames = 0; frames < 400 && walker_height(&w) == below->height; ++frames)
		walker_update(&w, ahead, &area);
	CHECK_EQ(0, walker_height(&w)); // up on the entry
}

// A run across (0, 9) jumps the street at its south side onto (0, 10), level with it: the first
// street, a jump either way
static void a_run_jumps_the_first_street(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	const city_tower *from = city_tower_at(0, 9), *to = city_tower_at(0, 10);
	int frames;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	CHECK_EQ(from->height, to->height);
	walker_init_at(&w, corridor_point(&c, CORRIDOR_LENGTH + 250, from->across_min + 20), from->height, 0x8000);
	for (frames = 0; frames < 400 && !w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK(w.falling);
	CHECK_EQ(WALKER_RUN_SPEED, w.forward); // a jump, not a step off: you never stopped
	for (frames = 0; frames < 200 && w.falling; ++frames)
		walker_update(&w, ahead, &area);
	CHECK(!w.falling);
	CHECK_EQ(to->height, walker_height(&w));
	CHECK_EQ(0, w.stagger);
	CHECK(walker_position(&w).y >= to->across_min); // across the street
}

// Walking (not running: from 30 off, too near to get up to WALKER_MOVE_SPEED) to a street's edge,
// you stop at its lip, and pushing on never takes you over: there's nothing below a street to land on
static void a_street_never_lets_you_fall(void)
{
	corridor c;
	walker_area area;
	walker w;
	const walker_controls ahead = {0, 1, 0, 0, 0};
	const city_tower *t = city_tower_at(0, 9);

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	walker_init_at(&w, corridor_point(&c, CORRIDOR_LENGTH + 250, t->across_max - 30), t->height, 0x8000);
	run(&w, ahead, &area, 600);
	CHECK(!w.falling);
	CHECK_EQ(t->height, walker_height(&w));
	CHECK(walker_position(&w).y <= t->across_max);
}

/*
 * One move, played: stood on tower a's far side (30 in from the edge away from b, in the middle the
 * other way), facing b, forward held until you're stood on b's roof. Whether you got there, cleanly
 * (no stagger), within 600 frames. The way's along x or y: the towers line up
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
	// The way to b, and its yaw (0xC000 faces +x, 0x4000 -x, 0x8000 +y, 0 -y)
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

/*
 * The fewest moves from the entry to a tower, over the table's roofs (side by side, neither a guard
 * page): breadth first. Writes the towers on the way (column, row) and returns how many, 0 if it
 * can't be reached
 */
#define ROUTE_MAX CITY_TOWERS
static int route_to(int goal_column, int goal_row, signed char route[][2])
{
	static short back[CITY_TOWERS];
	static short todo[CITY_TOWERS];
	int head = 0, tail = 0, n = 0;
	const int start = CITY_ENTRY_ROW * CITY_COLUMNS, goal = goal_row * CITY_COLUMNS + goal_column;

	for (int i = 0; i < CITY_TOWERS; ++i)
		back[i] = -1;
	back[start] = (short)start;
	todo[tail++] = (short)start;
	while (head < tail)
	{
		const int k = todo[head++];
		static const signed char ways[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

		for (int w = 0; w < 4; ++w)
		{
			const int c = k % CITY_COLUMNS + ways[w][0], r = k / CITY_COLUMNS + ways[w][1];
			const int n2 = r * CITY_COLUMNS + c;

			if (c < 0 || c >= CITY_COLUMNS || r < 0 || r >= CITY_ROWS || back[n2] >= 0 ||
				city_towers[n2].kind == CITY_GUARD)
				continue;
			back[n2] = (short)k;
			todo[tail++] = (short)n2;
		}
	}
	if (back[goal] < 0)
		return 0;
	for (int k = goal; k != start; k = back[k])
		++n;
	++n;
	for (int k = goal, i = n - 1; i >= 0; k = back[k], --i)
	{
		route[i][0] = (signed char)(k % CITY_COLUMNS);
		route[i][1] = (signed char)(k / CITY_COLUMNS);
	}
	return n;
}

/*
 * The way to each of the three ways out, played: from the entry to the hub, every move run as you'd
 * run it, and then all the way back to the entry. Every street on the way is level, every step in a
 * block a clean drop or a climb (every_move_works_both_ways), so both ways must be runnable
 */
static void the_way_to_each_hub_and_back_is_runnable(void)
{
	static const signed char hubs[3][2] = {{CITY_HUB_COLUMN, CITY_HUB_ROW}, {CITY_BACKUP_COLUMN, CITY_BACKUP_ROW}, {14, 14}};
	static signed char route[ROUTE_MAX][2];
	corridor c;
	walker_area area;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	for (int h = 0; h < 3; ++h)
	{
		const int steps = route_to(hubs[h][0], hubs[h][1], route);

		CHECK(steps > 1);
		for (int i = 1; i < steps; ++i)
		{
			const bool there = run_across(&c, &area, city_tower_at(route[i - 1][0], route[i - 1][1]),
										  city_tower_at(route[i][0], route[i][1]));
			const bool back = run_across(&c, &area, city_tower_at(route[i][0], route[i][1]),
										 city_tower_at(route[i - 1][0], route[i - 1][1]));

			if (!there || !back)
				printf("    (%X, %X) <-> (%X, %X): %s failed\n", route[i - 1][0], route[i - 1][1], route[i][0],
					   route[i][1], there ? "back" : "there");
			CHECK(there);
			CHECK(back);
		}
	}
}

/*
 * The watchdogs' patrols (city.c, city_patrols) fly clear of every tower: along each leg, between
 * each two stops in turn (a loop's last back to its first), no tower under the drone's spread (its
 * rotors reach 190 out: drone_draw.c) stands within 50 of the lower of the two stops' heights
 * (its hull hangs ~24 under its middle). A drone can weave and swing wide of the straight line a
 * little: the spread's taken as 250
 */
static void the_patrols_fly_clear_of_the_towers(void)
{
	for (int p = 0; p < CITY_WATCHDOGS; ++p)
	{
		const city_patrol *patrol = &city_patrols[p];

		CHECK(patrol->count >= 2 && patrol->count <= DRONE_STOPS_MAX);
		for (int i = 0; i < patrol->count; ++i)
		{
			const city_drone_place *a = &patrol->places[i];
			const city_drone_place *b = &patrol->places[(i + 1) % patrol->count];
			const int low = a->height < b->height ? a->height : b->height;

			if (patrol->back_and_forth && i + 1 == patrol->count)
				break; // back and forth: no leg from the last stop to the first
			for (int k = 0; k <= 32; ++k)
			{
				const int out = a->out + (b->out - a->out) * k / 32, across = a->across + (b->across - a->across) * k / 32;

				for (int t = 0; t < CITY_TOWERS; ++t)
				{
					const city_tower *tower = &city_towers[t];

					if (tower->out_max < out - 250 || tower->out_min > out + 250 || tower->across_max < across - 250 ||
						tower->across_min > across + 250)
						continue;
					if (tower->height > low - 50)
						printf("    patrol %d, stop %d to %d: a tower %d high under it, at %d\n", p, i, (i + 1) % patrol->count,
							   tower->height, low);
					CHECK(tower->height <= low - 50);
				}
			}
		}
	}
}

/*
 * Each frame the walker tests only the roofs it can reach (walker.c, WALKER_FRAME_REACH: the user,
 * 2026-11-18, the frames missed in the city). That must change nothing: random runs over the city,
 * every frame of them walked twice, against the frame's roofs and against all ~300, come out the
 * same to the last unit. Mostly running forwards, so they jump the streets, drop, climb and stop
 * at lips
 */
static unsigned int walk_seed = 18112026u;
static int walk_random(int n)
{
	walk_seed = walk_seed * 1103515245u + 12345u;
	return (int)((walk_seed >> 8) % (unsigned int)n);
}

static void the_frame_s_roofs_walk_as_all_of_them(void)
{
	corridor c;
	static walker_area area;
	int airborne = 0, frames = 0;

	open_round_five(&c);
	corridor_walker_area(&c, &area);
	for (int run_number = 0; run_number < 40; ++run_number)
	{
		const city_tower *t;
		world_point a, b, middle;
		walker narrow, every;

		do
			t = &city_towers[walk_random(CITY_TOWERS)];
		while (t->kind == CITY_GUARD);
		city_tower_corners(&c, t, &a, &b);
		middle.x = (short)((a.x + b.x) >> 1);
		middle.y = (short)((a.y + b.y) >> 1);
		walker_init_at(&narrow, middle, t->height, walk_random(4) * 0x4000);
		every = narrow;
		for (int hold = 0; hold < 30; ++hold)
		{
			// A press held 10-60 frames: forwards 3 times in 4, turning now and then
			walker_controls pad = {0, 0, 0, 0, 0};
			const int frames_held = 10 + walk_random(50);

			pad.move = (signed char)(walk_random(4) != 0 ? 1 : walk_random(3) - 1);
			pad.turn = (signed char)(walk_random(3) == 0 ? walk_random(3) - 1 : 0);
			pad.strafe = (signed char)(walk_random(5) == 0 ? walk_random(3) - 1 : 0);
			for (int f = 0; f < frames_held; ++f, ++frames)
			{
				walker_frame_narrowing = 1;
				walker_update(&narrow, pad, &area);
				walker_frame_narrowing = 0;
				walker_update(&every, pad, &area);
				walker_frame_narrowing = 1;
				airborne += narrow.falling ? 1 : 0;
				if (narrow.x != every.x || narrow.y != every.y || narrow.z != every.z || narrow.falling != every.falling ||
					narrow.move != every.move || narrow.slide != every.slide || narrow.forward != every.forward)
				{
					printf("    run %d, frame %d: (%d, %d, %d) narrowed, (%d, %d, %d) against all\n", run_number, f,
						   narrow.x >> WALKER_SUB_SHIFT, narrow.y >> WALKER_SUB_SHIFT, narrow.z >> WALKER_SUB_SHIFT,
						   every.x >> WALKER_SUB_SHIFT, every.y >> WALKER_SUB_SHIFT, every.z >> WALKER_SUB_SHIFT);
					CHECK(false);
					return;
				}
			}
		}
	}
	CHECK(frames > 40 * 30 * 10);
	CHECK(airborne > 200); // (it jumped and dropped: the flights were tested too)
}

void run_city_tests(void)
{
	RUN(the_frame_s_roofs_walk_as_all_of_them);
	RUN(the_patrols_fly_clear_of_the_towers);
	RUN(the_towers_are_on_the_plans_grid);
	RUN(every_move_works_both_ways);
	RUN(the_corridor_has_room_for_the_city);
	RUN(out_of_the_corridor_onto_the_entry);
	RUN(off_the_entry_a_clean_drop);
	RUN(off_the_entry_by_the_yard_wall);
	RUN(a_hub_s_roof_is_safe);
	RUN(a_drop_off_the_entry_climbs_back);
	RUN(a_run_jumps_the_first_street);
	RUN(a_street_never_lets_you_fall);
	RUN(the_way_to_each_hub_and_back_is_runnable);
}
