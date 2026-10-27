#include "arena.h"

static int pieces_per_side = ARENA_PIECES_DEFAULT;

int arena_set_pieces(int pieces)
{
	if (pieces < ARENA_PIECES_MIN)
		pieces = ARENA_PIECES_MIN;
	if (pieces > ARENA_PIECES_MAX)
		pieces = ARENA_PIECES_MAX;
	pieces_per_side = pieces;
	return pieces;
}

int arena_pieces(void)
{
	return pieces_per_side;
}

int arena_half(void)
{
	return pieces_per_side * ARENA_PIECE_HALF;
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
	if (last > pieces_per_side - 1)
		last = pieces_per_side - 1;
	if (last < *first)
		return 0;
	return last - *first + 1;
}
