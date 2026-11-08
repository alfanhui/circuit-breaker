#include "arena.h"

static int pieces_per_side = ARENA_PIECES_DEFAULT;
// How far every wall is pulled in from pieces_per_side's square (the last round, closing in),
// and the walls' column grid's reach for that (grid_pieces 250 pieces from -grid_half)
static int inset = 0;
static int grid_half = ARENA_PIECES_DEFAULT * ARENA_PIECE_HALF;
static int grid_pieces = ARENA_PIECES_DEFAULT;

// The grid the walls' columns stand on: from the middle, in whole 250 pieces, out to the first
// column at or beyond each wall. A division, but only when the size or inset changes (once a
// frame at most, while the walls move)
static void lay_out_grid(void)
{
	const int width = 2 * ARENA_PIECE_HALF;

	grid_half = (arena_half() + width - 1) / width * width;
	grid_pieces = 2 * grid_half / width;
}

// The layout, and its blocks laid out for the current size (lay_out_blocks): worked out when
// either changes, not every frame, as collision asks for them many times a frame
static int layout = ARENA_OPEN;
static world_rect blocks[ARENA_BLOCKS_MAX];
static int block_count = 0;
static int unit = 1000; // the default size's (lay_out_blocks)

// One block, from its corners in any order
static void add_block(int x0, int y0, int x1, int y1)
{
	world_rect *b = &blocks[block_count++];

	b->x_min = (short)(x0 < x1 ? x0 : x1);
	b->x_max = (short)(x0 < x1 ? x1 : x0);
	b->y_min = (short)(y0 < y1 ? y0 : y1);
	b->y_max = (short)(y0 < y1 ? y1 : y0);
}

static void lay_out_blocks(void)
{
	const int half = arena_half();
	const int t = ARENA_INNER_WALL_HALF;

	// An eighth of the half, rounded down to whole wall pieces so every block's faces are whole
	// pieces (level.c draws them as the outer walls are). A division, but only when the size
	// or layout changes
	unit = (half >> 3) / (2 * ARENA_PIECE_HALF) * (2 * ARENA_PIECE_HALF);
	block_count = 0;
	if (unit < ARENA_UNIT_MIN)
		return; // a debug arena too small for a layout: just the square
	switch (layout)
	{
	case ARENA_DONUT:
		add_block(-3 * unit, -3 * unit, 3 * unit, 3 * unit);
		break;
	case ARENA_PILLARS:
		// One in each quarter, 2 to 5 units out on both axes
		for (int sx = -1; sx <= 1; sx += 2)
			for (int sy = -1; sy <= 1; sy += 2)
				add_block(sx * 2 * unit, sy * 2 * unit, sx * 5 * unit, sy * 5 * unit);
		break;
	case ARENA_ROOMS:
		// The cross's two lines, each cut by a door 3 to 5 units out on either side of the
		// middle. They run right to the outer walls (half, not 8 units: a rounded-down unit
		// would leave a gap there). Their middle pieces cross over each other: stopping one
		// short at the other's side (125 from the middle) wouldn't be whole wall pieces
		add_block(-t, -half, t, -5 * unit);
		add_block(-t, -3 * unit, t, 3 * unit);
		add_block(-t, 5 * unit, t, half);
		add_block(-half, -t, -5 * unit, t);
		add_block(-3 * unit, -t, 3 * unit, t);
		add_block(5 * unit, -t, half, t);
		break;
	default: // ARENA_OPEN
		break;
	}
}

int arena_set_pieces(int pieces)
{
	if (pieces < ARENA_PIECES_MIN)
		pieces = ARENA_PIECES_MIN;
	if (pieces > ARENA_PIECES_MAX)
		pieces = ARENA_PIECES_MAX;
	pieces_per_side = pieces;
	if (inset > pieces * ARENA_PIECE_HALF - ARENA_INSET_LEAVES)
		inset = pieces * ARENA_PIECE_HALF - ARENA_INSET_LEAVES;
	if (inset < 0)
		inset = 0;
	lay_out_grid();
	lay_out_blocks();
	return pieces;
}

void arena_set_layout(int which)
{
	layout = which >= 0 && which < ARENA_LAYOUT_COUNT ? which : ARENA_OPEN;
	lay_out_blocks();
}

int arena_layout(void)
{
	return layout;
}

const world_rect *arena_blocks(int *count)
{
	*count = block_count;
	return blocks;
}

int arena_unit(void)
{
	return unit;
}

world_point arena_start(void)
{
	world_point p = {0, 0}; // the middle: clear in the open square and among the pillars

	if (layout == ARENA_DONUT)
		p.y = (short)(6 * unit); // in the south lane, half way across it
	else if (layout == ARENA_ROOMS)
	{
		p.x = (short)(4 * unit); // the middle of the south-west room (west is +x, south +y)
		p.y = (short)(4 * unit);
	}
	return p;
}

int arena_start_heading(void)
{
	// Along the donut's lane, not into the block; north everywhere else (as the flight down lands)
	return layout == ARENA_DONUT ? HEADING_WEST : HEADING_NORTH;
}

static int clamp(int v, int limit)
{
	return v > limit ? limit : v < -limit ? -limit : v;
}

world_point arena_clear_spot(world_point p, int margin)
{
	const int limit = arena_player_limit() - margin;
	// Clear of a block by as much as of the outer walls: their clearance, then the margin
	const int keep = ARENA_WALL_CLEARANCE + margin;
	int x = clamp(p.x, limit);
	int y = clamp(p.y, limit);

	for (int i = 0; i < block_count; ++i)
	{
		const world_rect *b = &blocks[i];
		// How far it would have to move out through each side of the block (grown by keep)
		int to_west = b->x_max + keep - x; // +x
		int to_east = x - (b->x_min - keep);
		int to_south = b->y_max + keep - y; // +y
		int to_north = y - (b->y_min - keep);
		int best;

		if (to_west <= 0 || to_east <= 0 || to_south <= 0 || to_north <= 0)
			continue; // outside it
		// Out the nearest side. Pushed out of one block it could land in another (the rooms'
		// cross, by its middle), but riders are only ever placed near the start, in the open
		best = to_west;
		if (to_east < best)
			best = to_east;
		if (to_south < best)
			best = to_south;
		if (to_north < best)
			best = to_north;
		if (best == to_west)
			x = b->x_max + keep;
		else if (best == to_east)
			x = b->x_min - keep;
		else if (best == to_south)
			y = b->y_max + keep;
		else
			y = b->y_min - keep;
	}
	p.x = (short)clamp(x, limit);
	p.y = (short)clamp(y, limit);
	return p;
}

int arena_pieces(void)
{
	return pieces_per_side;
}

int arena_set_inset(int units)
{
	const int most = pieces_per_side * ARENA_PIECE_HALF - ARENA_INSET_LEAVES;

	if (units > most)
		units = most;
	if (units < 0)
		units = 0;
	if (units == inset)
		return units;
	inset = units;
	lay_out_grid();
	if (layout != ARENA_OPEN)
		lay_out_blocks(); // a layout scales with the square (none closes in yet)
	return units;
}

int arena_inset(void)
{
	return inset;
}

int arena_half(void)
{
	return pieces_per_side * ARENA_PIECE_HALF - inset;
}

int arena_grid_half(void)
{
	return grid_half;
}

int arena_grid_pieces(void)
{
	return grid_pieces;
}

int arena_player_limit(void)
{
	return arena_half() - ARENA_WALL_CLEARANCE;
}

static int abs_int(int v)
{
	return v < 0 ? -v : v;
}

// How far the eye is from column k: the bigger of the gaps along and across the wall
static int column_distance(int side_start, int dir, int k, int eye_along, int eye_across)
{
	int along = abs_int(side_start + dir * k * 2 * ARENA_PIECE_HALF - eye_along);
	return along > eye_across ? along : eye_across;
}

int arena_wall_columns(int side_start, int dir, int first, int count, int eye_along, int eye_across,
					   short *along, int max_columns)
{
	int end = first + count;
	int n = 0;
	int k = first;

	if (eye_across < 0)
		eye_across = -eye_across;
	while (n < max_columns)
	{
		along[n++] = (short)(side_start + dir * k * 2 * ARENA_PIECE_HALF);
		if (k >= end)
			break;
		// The longest step whose nearer end is still far enough away for that level of detail
		int step = 8;
		int next = k + step < end ? k + step : end;
		int near = column_distance(side_start, dir, k, eye_along, eye_across);
		int far_end = column_distance(side_start, dir, next, eye_along, eye_across);
		int closest = near < far_end ? near : far_end;

		if (closest < ARENA_LOD_NEAR)
			step = 1;
		else if (closest < ARENA_LOD_MID)
			step = 4;
		// A short step must also not run into the near band
		if (step == 4 && column_distance(side_start, dir, k + 4 < end ? k + 4 : end, eye_along, eye_across) < ARENA_LOD_NEAR)
			step = 1;
		k = k + step < end ? k + step : end;
	}
	return n;
}

int arena_visible_pieces(int side_start, int dir, int along_min, int along_max, int *first)
{
	return arena_visible_pieces_of(side_start, dir, grid_pieces, along_min, along_max, first);
}

int arena_visible_pieces_of(int side_start, int dir, int pieces, int along_min, int along_max, int *first)
{
	const int width = 2 * ARENA_PIECE_HALF;
	// Distances along the side (from its first column, in its direction) of both ends of the span
	int a = (along_min - side_start) * dir;
	int b = (along_max - side_start) * dir;
	int from = a < b ? a : b;
	int to = a < b ? b : a;
	int last;

	// Convert distances to piece indices. Clamp before dividing: C rounds negatives towards 0
	*first = from <= 0 ? 0 : from / width;
	last = to <= 0 ? -1 : to / width; // the piece containing 'to' is still in range
	if (last > pieces - 1)
		last = pieces - 1;
	if (last < *first)
		return 0;
	return last - *first + 1;
}
