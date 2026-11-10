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
	TOWER(0, 0, -1460, CITY_ROOF), TOWER(1, 0, -1400, CITY_HUB), TOWER(2, 0, -1350, CITY_ROOF), TOWER(3, 0, -1310, CITY_ROOF), TOWER(4, 0, 400, CITY_GUARD), TOWER(5, 0, -970, CITY_SEALED), TOWER(6, 0, -750, CITY_SEALED), // row 0
	TOWER(0, 1, -1400, CITY_ROOF), TOWER(1, 1, -1360, CITY_ROOF), TOWER(2, 1, -1300, CITY_ROOF), TOWER(3, 1, -1260, CITY_ROOF), TOWER(4, 1, 400, CITY_GUARD), TOWER(5, 1, -1090, CITY_SEALED), TOWER(6, 1, 400, CITY_GUARD), // row 1
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
