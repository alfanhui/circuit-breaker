#include "city.h"

// A tower's corner on the plan: its column (east, out from the corridor's end) and row (south,
// across), as tools/work_ram_plan.py's corner(): 500 a tower, and a street (CITY_STREET) after
// every second one. Row 5's top edge is on the corridor's middle line (city.h)
#define S CITY_TOWER_SIZE
#define COL(c) (CORRIDOR_LENGTH + (c) * S + ((c) >> 1) * CITY_STREET)
#define ROW(r) ((r) * S + ((r) >> 1) * CITY_STREET - (5 * S + 2 * CITY_STREET))
#define TOWER(c, r, h, k) {COL(c), COL(c) + S, ROW(r), ROW(r) + S, h, k}

// The plan's towers and heights (city.h has them as a map). Generated: python
// tools/work_ram_plan.py --c. Edit the plan's map, not this
const city_tower city_towers[CITY_TOWERS] = {
	TOWER(0, 0, -1460, CITY_ROOF), TOWER(1, 0, -1400, CITY_HUB), TOWER(2, 0, -1400, CITY_ROOF), TOWER(3, 0, -1310, CITY_ROOF), TOWER(4, 0, 400, CITY_GUARD), TOWER(5, 0, -970, CITY_SEALED), TOWER(6, 0, -750, CITY_SEALED), // row 0
	TOWER(0, 1, -1400, CITY_ROOF), TOWER(1, 1, -1360, CITY_ROOF), TOWER(2, 1, -1360, CITY_ROOF), TOWER(3, 1, -1260, CITY_ROOF), TOWER(4, 1, 400, CITY_GUARD), TOWER(5, 1, -1090, CITY_SEALED), TOWER(6, 1, 400, CITY_GUARD), // row 1
	TOWER(0, 2, -1280, CITY_SEALED), TOWER(1, 2, 400, CITY_GUARD), TOWER(2, 2, 400, CITY_GUARD), TOWER(3, 2, -1210, CITY_ROOF), TOWER(4, 2, 400, CITY_GUARD), TOWER(5, 2, -860, CITY_SEALED), TOWER(6, 2, -1150, CITY_ROOF), // row 2
	TOWER(0, 3, 400, CITY_GUARD), TOWER(1, 3, 400, CITY_GUARD), TOWER(2, 3, 400, CITY_GUARD), TOWER(3, 3, -1160, CITY_ROOF), TOWER(4, 3, 400, CITY_GUARD), TOWER(5, 3, -980, CITY_SEALED), TOWER(6, 3, -1100, CITY_ROOF), // row 3
	TOWER(0, 4, -60, CITY_ROOF), TOWER(1, 4, -100, CITY_ROOF), TOWER(2, 4, 400, CITY_GUARD), TOWER(3, 4, -1100, CITY_ROOF), TOWER(4, 4, -980, CITY_SEALED), TOWER(5, 4, -860, CITY_SEALED), TOWER(6, 4, 400, CITY_GUARD), // row 4
	TOWER(0, 5, 0, CITY_ROOF), TOWER(1, 5, -50, CITY_ROOF), TOWER(2, 5, 400, CITY_GUARD), TOWER(3, 5, -1050, CITY_ROOF), TOWER(4, 5, 400, CITY_GUARD), TOWER(5, 5, 400, CITY_GUARD), TOWER(6, 5, 400, CITY_GUARD), // row 5 (the entry)
	TOWER(0, 6, -60, CITY_ROOF), TOWER(1, 6, -110, CITY_ROOF), TOWER(2, 6, 400, CITY_GUARD), TOWER(3, 6, -1000, CITY_ROOF), TOWER(4, 6, -950, CITY_ROOF), TOWER(5, 6, -900, CITY_ROOF), TOWER(6, 6, -860, CITY_ROOF), // row 6
	TOWER(0, 7, -100, CITY_ROOF), TOWER(1, 7, -160, CITY_ROOF), TOWER(2, 7, 400, CITY_GUARD), TOWER(3, 7, 400, CITY_GUARD), TOWER(4, 7, 400, CITY_GUARD), TOWER(5, 7, 400, CITY_GUARD), TOWER(6, 7, -810, CITY_ROOF), // row 7
	TOWER(0, 8, -160, CITY_ROOF), TOWER(1, 8, -210, CITY_ROOF), TOWER(2, 8, 400, CITY_GUARD), TOWER(3, 8, -540, CITY_SEALED), TOWER(4, 8, -660, CITY_ROOF), TOWER(5, 8, -700, CITY_ROOF), TOWER(6, 8, -750, CITY_ROOF), // row 8
	TOWER(0, 9, -200, CITY_ROOF), TOWER(1, 9, -260, CITY_ROOF), TOWER(2, 9, -310, CITY_ROOF), TOWER(3, 9, 400, CITY_GUARD), TOWER(4, 9, -600, CITY_ROOF), TOWER(5, 9, 400, CITY_GUARD), TOWER(6, 9, -810, CITY_ROOF), // row 9
	TOWER(0, 10, -260, CITY_ROOF), TOWER(1, 10, 400, CITY_GUARD), TOWER(2, 10, -360, CITY_ROOF), TOWER(3, 10, 400, CITY_GUARD), TOWER(4, 10, -560, CITY_ROOF), TOWER(5, 10, 400, CITY_GUARD), TOWER(6, 10, -610, CITY_SEALED), // row 10
	TOWER(0, 11, -300, CITY_ROOF), TOWER(1, 11, -350, CITY_ROOF), TOWER(2, 11, -400, CITY_ROOF), TOWER(3, 11, -450, CITY_HUB), TOWER(4, 11, -510, CITY_ROOF), TOWER(5, 11, 400, CITY_GUARD), TOWER(6, 11, -730, CITY_SEALED), // row 11
};

const city_tower *city_tower_at(int column, int row)
{
	return &city_towers[row * CITY_COLUMNS + column];
}

void city_tower_corners(const corridor *c, const city_tower *t, world_point *a, world_point *b)
{
	// The middle line: halfway across. A shift, not a division
	const int middle = (c->across_min + c->across_max) >> 1;

	*a = corridor_point(c, t->out_min, middle + t->across_min);
	*b = corridor_point(c, t->out_max, middle + t->across_max);
}

world_point city_hub_point(int side, int wall, int middle, int out, int across)
{
	const city_tower *hub = city_tower_at(CITY_HUB_COLUMN, CITY_HUB_ROW);

	return corridor_point_behind(side, wall, ((hub->out_min + hub->out_max) >> 1) + out,
								 middle + ((hub->across_min + hub->across_max) >> 1) + across);
}

world_point city_hub_middle(int side, int wall, int middle)
{
	return city_hub_point(side, wall, middle, 0, 0);
}

int city_seen_patch(const city_tower *roof, int out, int across)
{
	// Its quarters: 250 each way (CITY_TOWER_SIZE / CITY_SEEN_PATCHES). An edge between two
	// belongs to the further one: both saw it, in the table
	const int half = CITY_TOWER_SIZE / CITY_SEEN_PATCHES;

	return (out - roof->out_min >= half ? 1 : 0) + (across - roof->across_min >= half ? CITY_SEEN_PATCHES : 0);
}

int city_sees(int from, int patch, int tower)
{
	return (city_seen[from][patch][tower >> 3] >> (tower & 7)) & 1;
}

int city_roof_under(int out, int across, int z)
{
	for (int i = 0; i < CITY_TOWERS; ++i)
	{
		const city_tower *t = &city_towers[i];

		if (out >= t->out_min && out <= t->out_max && across >= t->across_min && across <= t->across_max)
			return z >= t->height && z <= t->height + CITY_SEEN_EYE ? i : -1;
	}
	return -1;
}

int city_walker_area_add(const corridor *c, walker_area *area)
{
	for (int i = 0; i < CITY_TOWERS; ++i)
	{
		world_point a, b;

		city_tower_corners(c, &city_towers[i], &a, &b);
		if (!walker_area_add_roof(area, a.x, a.y, b.x, b.y, city_towers[i].height))
			return 0;
	}
	return 1;
}

void city_drone_route(const corridor *c, drone_stop *route)
{
	// Between the plan's columns 1 and 2: the street's middle (a shift: no division), less
	// CITY_DRONE_IN: its tail clear of the guard pages, its nose out over column 1's lower roofs
	const int out = ((city_tower_at(1, 6)->out_max + city_tower_at(2, 6)->out_min) >> 1) - CITY_DRONE_IN;
	const int middle = (c->across_min + c->across_max) >> 1;
	// Facing back the way the corridor came: its heading's opposite (side is a heading, outwards)
	const unsigned char back = (unsigned char)((c->side + 2) & 3);
	// Each floor's stop: across, where two rows of a block meet (the row's far edge), and how high.
	// Rows 10-11: column 1's a guard page there, so column 0's edge (the rows line up)
	static const signed char rows[3] = {6, 8, 10};
	// High over the roofs it watches (the user, 2026-11-14: higher, its scan angled down at them):
	// 300-400 over rows 6-11's roofs (-60 .. -400), each stop a little lower than the last
	static const short heights[3] = {320, 170, 60};
	static const unsigned char order[CITY_DRONE_STOPS] = {0, 1, 2, 1}; // down, and back up

	for (int i = 0; i < CITY_DRONE_STOPS; ++i)
	{
		const int floor = order[i];
		const world_point at = corridor_point(c, out, middle + city_tower_at(0, rows[floor])->across_max);

		route[i].x = at.x;
		route[i].y = at.y;
		route[i].z = heights[floor];
		route[i].heading = back;
		route[i].wait = CITY_DRONE_WAIT;
	}
}

void city_drone_boxes(const corridor *c, drone_box *boxes)
{
	for (int i = 0; i < CITY_TOWERS; ++i)
	{
		world_point a, b;

		city_tower_corners(c, &city_towers[i], &a, &b);
		boxes[i].x_min = a.x < b.x ? a.x : b.x;
		boxes[i].x_max = a.x < b.x ? b.x : a.x;
		boxes[i].y_min = a.y < b.y ? a.y : b.y;
		boxes[i].y_max = a.y < b.y ? b.y : a.y;
		boxes[i].top = city_towers[i].height;
	}
}
