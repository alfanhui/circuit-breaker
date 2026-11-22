#include "city.h"

// A tower's corner on the plan: its column (out from the corridor's end) and row (across), as
// tools/ram_banks_plan.py's tower_rect: 500 a tower, and a street (CITY_STREET) after every second
// one. Row CITY_ENTRY_ROW's top edge is on the corridor's middle line (city.h)
#define S CITY_TOWER_SIZE
#define COL(c) (CORRIDOR_LENGTH + (c) * S + ((c) >> 1) * CITY_STREET)
#define ROW(r) ((r) * S + ((r) >> 1) * CITY_STREET - (CITY_ENTRY_ROW * S + (CITY_ENTRY_ROW >> 1) * CITY_STREET))
#define TOWER(c, r, h, k) {COL(c), COL(c) + S, ROW(r), ROW(r) + S, h, k}

// Low Work RAM's towers and heights (city.h has the plan). Generated: python
// tools/ram_banks_plan.py --quick --c. Edit the plan's map (LOW, in that file), not this
const city_tower city_towers[CITY_TOWERS] = {
// tools/ram_banks_plan.py --c: Low Work RAM, 16 x 16, row by row
	TOWER(0, 0, -360, CITY_ROOF), TOWER(1, 0, -360, CITY_ROOF), TOWER(2, 0, -360, CITY_ROOF), TOWER(3, 0, 130, CITY_GUARD), TOWER(4, 0, -360, CITY_HUB), TOWER(5, 0, -360, CITY_ROOF), TOWER(6, 0, -360, CITY_ROOF), TOWER(7, 0, -360, CITY_ROOF), TOWER(8, 0, -360, CITY_ROOF), TOWER(9, 0, 40, CITY_GUARD), TOWER(10, 0, -450, CITY_ROOF), TOWER(11, 0, -540, CITY_ROOF), TOWER(12, 0, -540, CITY_ROOF), TOWER(13, 0, -140, CITY_GUARD), TOWER(14, 0, -630, CITY_ROOF), TOWER(15, 0, -230, CITY_GUARD), // row 0
	TOWER(0, 1, -270, CITY_ROOF), TOWER(1, 1, 220, CITY_GUARD), TOWER(2, 1, -270, CITY_ROOF), TOWER(3, 1, 130, CITY_GUARD), TOWER(4, 1, -270, CITY_ROOF), TOWER(5, 1, 130, CITY_GUARD), TOWER(6, 1, -270, CITY_ROOF), TOWER(7, 1, 130, CITY_GUARD), TOWER(8, 1, -360, CITY_ROOF), TOWER(9, 1, 40, CITY_GUARD), TOWER(10, 1, -450, CITY_ROOF), TOWER(11, 1, -50, CITY_GUARD), TOWER(12, 1, -540, CITY_ROOF), TOWER(13, 1, -140, CITY_GUARD), TOWER(14, 1, -630, CITY_ROOF), TOWER(15, 1, -230, CITY_GUARD), // row 1
	TOWER(0, 2, -270, CITY_ROOF), TOWER(1, 2, 220, CITY_GUARD), TOWER(2, 2, -270, CITY_ROOF), TOWER(3, 2, -270, CITY_ROOF), TOWER(4, 2, -270, CITY_ROOF), TOWER(5, 2, 220, CITY_GUARD), TOWER(6, 2, -270, CITY_ROOF), TOWER(7, 2, -360, CITY_ROOF), TOWER(8, 2, -360, CITY_ROOF), TOWER(9, 2, -450, CITY_ROOF), TOWER(10, 2, -450, CITY_ROOF), TOWER(11, 2, -50, CITY_GUARD), TOWER(12, 2, -540, CITY_ROOF), TOWER(13, 2, -630, CITY_ROOF), TOWER(14, 2, -630, CITY_ROOF), TOWER(15, 2, -230, CITY_GUARD), // row 2
	TOWER(0, 3, -180, CITY_ROOF), TOWER(1, 3, 310, CITY_GUARD), TOWER(2, 3, 310, CITY_GUARD), TOWER(3, 3, 310, CITY_GUARD), TOWER(4, 3, 310, CITY_GUARD), TOWER(5, 3, 220, CITY_GUARD), TOWER(6, 3, 220, CITY_GUARD), TOWER(7, 3, 130, CITY_GUARD), TOWER(8, 3, -360, CITY_ROOF), TOWER(9, 3, 40, CITY_GUARD), TOWER(10, 3, 40, CITY_GUARD), TOWER(11, 3, -50, CITY_GUARD), TOWER(12, 3, -50, CITY_GUARD), TOWER(13, 3, -140, CITY_GUARD), TOWER(14, 3, -630, CITY_ROOF), TOWER(15, 3, -230, CITY_GUARD), // row 3
	TOWER(0, 4, -180, CITY_ROOF), TOWER(1, 4, -180, CITY_ROOF), TOWER(2, 4, -180, CITY_ROOF), TOWER(3, 4, 310, CITY_GUARD), TOWER(4, 4, -180, CITY_ROOF), TOWER(5, 4, -270, CITY_ROOF), TOWER(6, 4, -270, CITY_ROOF), TOWER(7, 4, 130, CITY_GUARD), TOWER(8, 4, -360, CITY_ROOF), TOWER(9, 4, -450, CITY_ROOF), TOWER(10, 4, -450, CITY_ROOF), TOWER(11, 4, -50, CITY_GUARD), TOWER(12, 4, -540, CITY_ROOF), TOWER(13, 4, -630, CITY_ROOF), TOWER(14, 4, -630, CITY_ROOF), TOWER(15, 4, -230, CITY_GUARD), // row 4
	TOWER(0, 5, -90, CITY_ROOF), TOWER(1, 5, 310, CITY_GUARD), TOWER(2, 5, -90, CITY_ROOF), TOWER(3, 5, 310, CITY_GUARD), TOWER(4, 5, -180, CITY_ROOF), TOWER(5, 5, 220, CITY_GUARD), TOWER(6, 5, -270, CITY_ROOF), TOWER(7, 5, 130, CITY_GUARD), TOWER(8, 5, -360, CITY_ROOF), TOWER(9, 5, 40, CITY_GUARD), TOWER(10, 5, 40, CITY_GUARD), TOWER(11, 5, -50, CITY_GUARD), TOWER(12, 5, -540, CITY_ROOF), TOWER(13, 5, -140, CITY_GUARD), TOWER(14, 5, -630, CITY_ROOF), TOWER(15, 5, -230, CITY_GUARD), // row 5
	TOWER(0, 6, -90, CITY_ROOF), TOWER(1, 6, 400, CITY_GUARD), TOWER(2, 6, -90, CITY_ROOF), TOWER(3, 6, -180, CITY_ROOF), TOWER(4, 6, -180, CITY_ROOF), TOWER(5, 6, -270, CITY_ROOF), TOWER(6, 6, -270, CITY_ROOF), TOWER(7, 6, 130, CITY_GUARD), TOWER(8, 6, -360, CITY_ROOF), TOWER(9, 6, -450, CITY_ROOF), TOWER(10, 6, -450, CITY_ROOF), TOWER(11, 6, -540, CITY_ROOF), TOWER(12, 6, -540, CITY_ROOF), TOWER(13, 6, -630, CITY_ROOF), TOWER(14, 6, -630, CITY_ROOF), TOWER(15, 6, -230, CITY_GUARD), // row 6
	TOWER(0, 7, 400, CITY_GUARD), TOWER(1, 7, 400, CITY_GUARD), TOWER(2, 7, -90, CITY_ROOF), TOWER(3, 7, 310, CITY_GUARD), TOWER(4, 7, 310, CITY_GUARD), TOWER(5, 7, 220, CITY_GUARD), TOWER(6, 7, -270, CITY_ROOF), TOWER(7, 7, 130, CITY_GUARD), TOWER(8, 7, 130, CITY_GUARD), TOWER(9, 7, 40, CITY_GUARD), TOWER(10, 7, 40, CITY_GUARD), TOWER(11, 7, -50, CITY_GUARD), TOWER(12, 7, -50, CITY_GUARD), TOWER(13, 7, -140, CITY_GUARD), TOWER(14, 7, -140, CITY_GUARD), TOWER(15, 7, -230, CITY_GUARD), // row 7
	TOWER(0, 8, 0, CITY_ROOF), TOWER(1, 8, 400, CITY_GUARD), TOWER(2, 8, -90, CITY_ROOF), TOWER(3, 8, -180, CITY_ROOF), TOWER(4, 8, -180, CITY_ROOF), TOWER(5, 8, 220, CITY_GUARD), TOWER(6, 8, -270, CITY_ROOF), TOWER(7, 8, -360, CITY_ROOF), TOWER(8, 8, -360, CITY_ROOF), TOWER(9, 8, -450, CITY_ROOF), TOWER(10, 8, -450, CITY_ROOF), TOWER(11, 8, -540, CITY_ROOF), TOWER(12, 8, -540, CITY_ROOF), TOWER(13, 8, -630, CITY_ROOF), TOWER(14, 8, -630, CITY_ROOF), TOWER(15, 8, -230, CITY_GUARD), // row 8
	TOWER(0, 9, -90, CITY_ROOF), TOWER(1, 9, 400, CITY_GUARD), TOWER(2, 9, 400, CITY_GUARD), TOWER(3, 9, 310, CITY_GUARD), TOWER(4, 9, 310, CITY_GUARD), TOWER(5, 9, 220, CITY_GUARD), TOWER(6, 9, 220, CITY_GUARD), TOWER(7, 9, 130, CITY_GUARD), TOWER(8, 9, -360, CITY_ROOF), TOWER(9, 9, 40, CITY_GUARD), TOWER(10, 9, -450, CITY_ROOF), TOWER(11, 9, -50, CITY_GUARD), TOWER(12, 9, -50, CITY_GUARD), TOWER(13, 9, -140, CITY_GUARD), TOWER(14, 9, -630, CITY_ROOF), TOWER(15, 9, -230, CITY_GUARD), // row 9
	TOWER(0, 10, -90, CITY_ROOF), TOWER(1, 10, -90, CITY_ROOF), TOWER(2, 10, -90, CITY_ROOF), TOWER(3, 10, 310, CITY_GUARD), TOWER(4, 10, -180, CITY_ROOF), TOWER(5, 10, -270, CITY_ROOF), TOWER(6, 10, -270, CITY_ROOF), TOWER(7, 10, 130, CITY_GUARD), TOWER(8, 10, -360, CITY_ROOF), TOWER(9, 10, -450, CITY_ROOF), TOWER(10, 10, -450, CITY_ROOF), TOWER(11, 10, -540, CITY_ROOF), TOWER(12, 10, -540, CITY_ROOF), TOWER(13, 10, -140, CITY_GUARD), TOWER(14, 10, -630, CITY_ROOF), TOWER(15, 10, -230, CITY_GUARD), // row A
	TOWER(0, 11, -180, CITY_ROOF), TOWER(1, 11, 310, CITY_GUARD), TOWER(2, 11, -180, CITY_ROOF), TOWER(3, 11, 310, CITY_GUARD), TOWER(4, 11, -180, CITY_ROOF), TOWER(5, 11, 220, CITY_GUARD), TOWER(6, 11, -270, CITY_ROOF), TOWER(7, 11, 130, CITY_GUARD), TOWER(8, 11, 130, CITY_GUARD), TOWER(9, 11, 40, CITY_GUARD), TOWER(10, 11, -450, CITY_ROOF), TOWER(11, 11, -50, CITY_GUARD), TOWER(12, 11, -50, CITY_GUARD), TOWER(13, 11, -140, CITY_GUARD), TOWER(14, 11, -630, CITY_ROOF), TOWER(15, 11, -230, CITY_GUARD), // row B
	TOWER(0, 12, -180, CITY_ROOF), TOWER(1, 12, -180, CITY_ROOF), TOWER(2, 12, -180, CITY_ROOF), TOWER(3, 12, -180, CITY_ROOF), TOWER(4, 12, -180, CITY_ROOF), TOWER(5, 12, -270, CITY_ROOF), TOWER(6, 12, -270, CITY_ROOF), TOWER(7, 12, -360, CITY_ROOF), TOWER(8, 12, -360, CITY_ROOF), TOWER(9, 12, 40, CITY_GUARD), TOWER(10, 12, -450, CITY_ROOF), TOWER(11, 12, -50, CITY_GUARD), TOWER(12, 12, -540, CITY_ROOF), TOWER(13, 12, -630, CITY_ROOF), TOWER(14, 12, -630, CITY_ROOF), TOWER(15, 12, -230, CITY_GUARD), // row C
	TOWER(0, 13, -270, CITY_ROOF), TOWER(1, 13, 220, CITY_GUARD), TOWER(2, 13, 220, CITY_GUARD), TOWER(3, 13, 220, CITY_GUARD), TOWER(4, 13, -270, CITY_ROOF), TOWER(5, 13, 220, CITY_GUARD), TOWER(6, 13, 220, CITY_GUARD), TOWER(7, 13, 130, CITY_GUARD), TOWER(8, 13, -360, CITY_ROOF), TOWER(9, 13, 40, CITY_GUARD), TOWER(10, 13, 40, CITY_GUARD), TOWER(11, 13, -50, CITY_GUARD), TOWER(12, 13, -540, CITY_ROOF), TOWER(13, 13, -140, CITY_GUARD), TOWER(14, 13, -630, CITY_ROOF), TOWER(15, 13, -230, CITY_GUARD), // row D
	TOWER(0, 14, -270, CITY_ROOF), TOWER(1, 14, -270, CITY_ROOF), TOWER(2, 14, -270, CITY_ROOF), TOWER(3, 14, -270, CITY_ROOF), TOWER(4, 14, -270, CITY_ROOF), TOWER(5, 14, 220, CITY_GUARD), TOWER(6, 14, -270, CITY_ROOF), TOWER(7, 14, -360, CITY_ROOF), TOWER(8, 14, -360, CITY_ROOF), TOWER(9, 14, -450, CITY_ROOF), TOWER(10, 14, -450, CITY_HUB), TOWER(11, 14, -540, CITY_ROOF), TOWER(12, 14, -540, CITY_ROOF), TOWER(13, 14, -140, CITY_GUARD), TOWER(14, 14, -630, CITY_HUB), TOWER(15, 14, -230, CITY_GUARD), // row E
	TOWER(0, 15, 130, CITY_GUARD), TOWER(1, 15, 130, CITY_GUARD), TOWER(2, 15, 130, CITY_GUARD), TOWER(3, 15, 130, CITY_GUARD), TOWER(4, 15, 130, CITY_GUARD), TOWER(5, 15, 130, CITY_GUARD), TOWER(6, 15, 130, CITY_GUARD), TOWER(7, 15, 130, CITY_GUARD), TOWER(8, 15, 130, CITY_GUARD), TOWER(9, 15, 40, CITY_GUARD), TOWER(10, 15, 40, CITY_GUARD), TOWER(11, 15, -50, CITY_GUARD), TOWER(12, 15, -50, CITY_GUARD), TOWER(13, 15, -140, CITY_GUARD), TOWER(14, 15, -140, CITY_GUARD), TOWER(15, 15, -230, CITY_GUARD), // row F
};

const city_tower *city_tower_at(int column, int row)
{
	return &city_towers[row * CITY_COLUMNS + column];
}

unsigned int city_tower_address(int column, int row)
{
	// Shifts: 64 KB a row, 4 KB a page
	return CITY_BASE + ((unsigned int)row << 16) + ((unsigned int)column << 12);
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

// city_seen_bit's shifts need 16 columns and whole blocks: a build error otherwise (an array of -1).
// (Not _Static_assert: the PC's tests build with MSVC's C, which hasn't got it)
typedef char city_seen_bit_needs_16_columns[(CITY_COLUMNS == 16 && (CITY_ROWS & 1) == 0) ? 1 : -1];

int city_seen_bit(int tower)
{
	const int c = tower & (CITY_COLUMNS - 1), r = tower >> 4;

	return (((r >> 1) * CITY_BLOCK_COLUMNS + (c >> 1)) << 2) + ((r & 1) << 1) + (c & 1);
}

int city_seen_has(const unsigned char *row, int tower)
{
	const int bit = city_seen_bit(tower);

	return (row[bit >> 3] >> (bit & 7)) & 1;
}

int city_sees(int from, int patch, int tower)
{
	return city_seen_has(city_seen[from][patch], tower);
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

// Which 250 of a block's 1000 (its two towers) w is in: 0 .. 3. Compares, not a division
static int city_quarter(int w)
{
	return w >= 3 * (S / 2) ? 3 : w >= 2 * (S / 2) ? 2 : w >= S / 2 ? 1 : 0;
}

int city_street_under(int out, int across, int z)
{
	// From the bank's corner (column 0, row 0): a block's two towers and the street after it, 1200
	const int block = 2 * S + CITY_STREET;
	int wo = out - COL(0), wa = across - ROW(0);
	int bx = 0, by = 0, piece;

	if (wo < 0 || wa < 0)
		return -1; // off the bank's near edges
	// Which block you're by, each way: at most 7 steps down, not a division
	while (wo >= block && bx < CITY_STREETS)
		wo -= block, ++bx;
	while (wa >= block && by < CITY_STREETS)
		wa -= block, ++by;
	// wo, wa: how far into that block, and the street after it (its last 200)
	{
		const bool out_street = wo >= 2 * S, across_street = wa >= 2 * S;

		if ((out_street && bx == CITY_STREETS) || (across_street && by == CITY_STREETS))
			return -1; // past the bank's far edges: there's no street there, nothing
		if (out_street && across_street)
			piece = 2 * CITY_STREETS * CITY_STREET_ALONG + by * CITY_STREETS + bx; // a crossing
		else if (out_street)
			piece = bx * CITY_STREET_ALONG + by * 4 + city_quarter(wa); // running across
		else if (across_street)
			piece = CITY_STREETS * CITY_STREET_ALONG + by * CITY_STREET_ALONG + bx * 4 + city_quarter(wo); // running out
		else
			return -1; // over a block's towers: a roof (city_roof_under), not a street
	}
	return z <= city_street_eye[piece] ? piece : -1;
}

bool city_on_hub(const corridor *c, world_point p, int feet)
{
	// The city's frame: out along the corridor, across from its middle line. Across is world x for
	// a corridor behind the north or south wall (it runs along y), world y for east or west
	// (corridor_point_behind)
	const bool along_y = c->side == WALL_FLICKER_NORTH || c->side == WALL_FLICKER_SOUTH;
	const int across = (along_y ? p.x : p.y) - ((c->across_min + c->across_max) >> 1);
	const int roof = city_roof_under(corridor_out(c, p), across, feet);

	return roof >= 0 && city_towers[roof].kind == CITY_HUB;
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

/*
 * The watchdogs' patrols (city.h, city_patrol), on the way from the entry to the CD block. That
 * way runs down column 0 to row A, along row C, and down column 8 to row E (tools/ram_banks_plan.py's
 * LOW map). Each stop is over a tower's middle (OVER: its column and row, as the map's), high enough
 * to clear the guard pages round it, which stand up to 400 near the entry and fall away with the
 * roofs towards the CD block. One stop a line: {out, across, height, CITY_FACES_..., wait}
 */
#define OVER(c, r, h, faces, wait) {COL(c) + S / 2, ROW(r) + S / 2, h, faces, wait}

/*
 * 1: a loop round the block just south of the entry (the guard page at column 1, row B), the first
 * you meet. Flying up column 0 to its first stop, its beam reaches just short of the entry roof
 * (a near miss of its own: WORK_RAM.md's warning is built on it). Waiting there it looks out along
 * row A, never back at the entry
 */
static const city_drone_place patrol_entry[] = {
	OVER(0, 10, 450, CITY_FACES_OUT, 60),
	OVER(2, 10, 450, CITY_FACES_SOUTH, 90),
	OVER(2, 12, 450, CITY_FACES_SOUTH, 120),
	OVER(0, 12, 450, CITY_FACES_NORTH, 90),
};

// 2: back and forth along row C, the crossroads at column 4 to the corner at column 8, looking
// down each junction's way in turn (south at 4, north at 6, south at 8: the way on to the CD block)
static const city_drone_place patrol_row_c[] = {
	OVER(4, 12, 400, CITY_FACES_SOUTH, 90),
	OVER(6, 12, 380, CITY_FACES_NORTH, 60),
	OVER(8, 12, 300, CITY_FACES_SOUTH, 120),
};

/*
 * 3: a loop round the CD block's lift (column A, row E), over the guard pages and the edge wall
 * round it (the lift's column is 120 wide: these are 500 and more off it), each stop looking out
 * at a way in: back along row E's west half and column 8, north, and east along row E
 */
static const city_drone_place patrol_lift[] = {
	OVER(9, 13, 200, CITY_FACES_BACK, 90),
	OVER(11, 13, 200, CITY_FACES_NORTH, 90),
	OVER(11, 15, 200, CITY_FACES_OUT, 90),
	OVER(9, 15, 200, CITY_FACES_BACK, 90),
};

#if CITY_WATCHDOG
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
const city_patrol city_patrols[CITY_WATCHDOGS] = {
	{patrol_entry, COUNT(patrol_entry), false},
	{patrol_row_c, COUNT(patrol_row_c), true},
	{patrol_lift, COUNT(patrol_lift), false},
};
// A patrol has no more stops than a route holds: checked as it's compiled (an array can't be -1
// long), as the PC's compiler for the tests has no _Static_assert in C
typedef char city_patrols_fit[COUNT(patrol_entry) <= DRONE_STOPS_MAX && COUNT(patrol_row_c) <= DRONE_STOPS_MAX &&
									  COUNT(patrol_lift) <= DRONE_STOPS_MAX
								  ? 1
								  : -1];
#endif

int city_faces_heading(const corridor *c, int faces)
{
	// The world's heading towards row F (+across): a unit step across, as corridor_point makes it
	const world_point a = corridor_point(c, 0, 0), b = corridor_point(c, 0, 1);
	const int south = b.x != a.x ? (b.x > a.x ? HEADING_WEST : HEADING_EAST) : (b.y > a.y ? HEADING_SOUTH : HEADING_NORTH);

	// Out is the corridor's side, back its opposite; north is south's opposite
	switch (faces & 3)
	{
	case CITY_FACES_OUT:
		return c->side;
	case CITY_FACES_SOUTH:
		return south;
	case CITY_FACES_BACK:
		return (c->side + 2) & 3;
	default:
		return (south + 2) & 3;
	}
}

int city_drone_route(const corridor *c, int patrol, drone_stop *route)
{
#if CITY_WATCHDOG
	const city_patrol *patrol_of = &city_patrols[patrol];
	const int middle = (c->across_min + c->across_max) >> 1;

	for (int i = 0; i < patrol_of->count; ++i)
	{
		const city_drone_place *p = &patrol_of->places[i];
		const world_point at = corridor_point(c, p->out, middle + p->across);

		route[i].x = at.x;
		route[i].y = at.y;
		route[i].z = p->height;
		route[i].heading = (unsigned char)city_faces_heading(c, p->faces);
		route[i].wait = p->wait;
	}
	return patrol_of->count;
#else
	(void)c;
	(void)patrol;
	(void)route;
	return 0;
#endif
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
