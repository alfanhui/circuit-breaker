#include "city.h"

// A tower's corner on the plan: its column (east, out from the corridor's end) and row (south,
// across), as tools/work_ram_plan.py's corner(): 500 a tower, and a street of 100 after every
// second one. Row 5's top edge is on the corridor's middle line (city.h)
#define S CITY_TOWER_SIZE
#define COL(c) (CORRIDOR_LENGTH + (c) * S + ((c) >> 1) * 100)
#define ROW(r) ((r) * S + ((r) >> 1) * 100 - (5 * S + 2 * 100))
#define TOWER(c, r, h) {COL(c), COL(c) + S, ROW(r), ROW(r) + S, h}
#define GUARD 400 // a guard page: far above everything near it

// The plan's heights for these 21 towers (city.h has them as a map), row by row
const city_tower city_towers[CITY_TOWERS] = {
	TOWER(0, 3, GUARD), TOWER(1, 3, GUARD), TOWER(2, 3, GUARD),
	TOWER(0, 4, 60),	TOWER(1, 4, 120),	TOWER(2, 4, GUARD), // sealed: out of reach
	TOWER(0, 5, 0),		TOWER(1, 5, -50),	TOWER(2, 5, GUARD), // the entry, level with the corridor
	TOWER(0, 6, -60),	TOWER(1, 6, -110),	TOWER(2, 6, GUARD),
	TOWER(0, 7, -100),	TOWER(1, 7, -160),	TOWER(2, 7, GUARD),
	TOWER(0, 8, -160),	TOWER(1, 8, -210),	TOWER(2, 8, GUARD),
	TOWER(0, 9, -200),	TOWER(1, 9, -260),	TOWER(2, 9, -310),
};

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
