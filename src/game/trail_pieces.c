#include "trail_pieces.h"

static int abs_int(int v)
{
	return v < 0 ? -v : v;
}

static int max_int(int a, int b)
{
	return a > b ? a : b;
}

static int min_int(int a, int b)
{
	return a < b ? a : b;
}

// "Chebyshev" distance: the bigger of the x and y gaps. The draw window is a square,
// so this is the distance that matters, and it needs no square root.
static int square_distance(world_point a, world_point b)
{
	return max_int(abs_int(a.x - b.x), abs_int(a.y - b.y));
}

/*
 * Positions along a segment are measured in steps s = 0..length along its longer axis,
 * so the point at s is from + (to - from) * s / length. For the usual axis-aligned
 * segment that's exact; for a diagonal one the shorter axis rounds by under a unit.
 */
static world_point point_at(world_point from, world_point to, int length, int s)
{
	world_point p;

	// Fast path: in play every segment runs straight along x or y, so the longer axis moves by
	// exactly s and the other not at all. The SH-2 has no divide instruction (a division is a
	// library call), and this runs for every column of every frame
	if (from.y == to.y)
	{
		p.x = (short)(to.x > from.x ? from.x + s : from.x - s);
		p.y = from.y;
		return p;
	}
	if (from.x == to.x)
	{
		p.x = from.x;
		p.y = (short)(to.y > from.y ? from.y + s : from.y - s);
		return p;
	}
	p.x = (short)(from.x + (to.x - from.x) * s / length); // diagonal: only the debug zig-zag's head
	p.y = (short)(from.y + (to.y - from.y) * s / length);
	return p;
}

// Narrows [*s0, *s1] to the steps where one axis stays within [min, max].
// start/delta are that axis's from value and total change. Returns 0 if nothing is left.
static int clip_axis(int start, int delta, int length, int min, int max, int *s0, int *s1)
{
	int low = min - start; // offsets from start that are inside
	int high = max - start;
	int a, b;

	if (delta == 0)
		return low <= 0 && 0 <= high; // constant on this axis: all in or all out

	// Offset = delta * s / length, so s = offset * length / delta (swap ends if delta < 0).
	// Along the segment's own axis delta is +-length: no division needed
	if (delta == length || delta == -length)
	{
		a = delta > 0 ? low : -low;
		b = delta > 0 ? high : -high;
	}
	else
	{
		a = low * length / delta;
		b = high * length / delta;
	}
	if (a > b)
	{
		int swap = a;
		a = b;
		b = swap;
	}
	*s0 = max_int(*s0, a);
	*s1 = min_int(*s1, b);
	return *s0 < *s1;
}

// How far along the segment (in steps from `from`) the focus is level with. It can be off the
// segment's ends, or negative: only its position on the grid of near pieces matters
static int focus_step(world_point from, world_point to, world_point focus)
{
	if (from.y == to.y)
		return to.x > from.x ? focus.x - from.x : from.x - focus.x;
	if (from.x == to.x)
		return to.y > from.y ? focus.y - from.y : from.y - focus.y;
	return 0; // diagonal (the debug zig-zag's head): no grid
}

/*
 * The end of the near piece starting at s. Near pieces are cut on a grid centred on the player:
 * boundaries at f +- 128, f +- 384, ... (f: focus_step), so one piece is centred level with it.
 * Why: VDP1 has no depth buffer, so SGL draws polygons back to front, judging each by its
 * centre. A 256-long piece beside the cycle whose centre was up to 128 further along could be
 * judged nearer or further than the cycle wrongly, and the cycle flickered in front of the
 * trail it should be hidden behind, at one spot (iso view, running right beside the trail).
 * The grid is 256 so that "how far past a boundary" is a mask (& 255) rather than a division.
 * (& on a negative int is still the right remainder: ints are two's complement on the SH-2 and PC.)
 */
static int next_near_boundary(int s, int f)
{
	return s + TRAIL_NEAR_PIECE - ((s - f - (TRAIL_NEAR_PIECE >> 1)) & (TRAIL_NEAR_PIECE - 1));
}

// One strip of pieces from step s0 to s1 along the segment
static int add_strip(world_point from, world_point to, int length, int f, world_point focus, int s0, int s1,
					 trail_column *out, int count, int max_columns)
{
	int s;

	if (s0 >= s1 || count + 2 > max_columns) // a strip needs at least two columns
		return count;

	out[count].p = point_at(from, to, length, s0);
	out[count].joined = 0;
	++count;

	for (s = s0; s < s1 && count < max_columns; ++count)
	{
		// Short pieces if either end of a long piece would be near the player
		int far_end = min_int(s + TRAIL_FAR_PIECE, s1);
		int near = square_distance(point_at(from, to, length, s), focus) < TRAIL_NEAR_RANGE ||
				   square_distance(point_at(from, to, length, far_end), focus) < TRAIL_NEAR_RANGE;

		s = min_int(near ? next_near_boundary(s, f) : s + TRAIL_FAR_PIECE, s1);
		out[count].p = point_at(from, to, length, s);
		out[count].joined = 1;
	}
	return count;
}

static int add_segment(world_point from, world_point to, world_point focus, const world_rect *view,
					   const trail_occluders *occluders, trail_column *out, int count, int max_columns)
{
	const int length = max_int(abs_int(to.x - from.x), abs_int(to.y - from.y));
	const int f = focus_step(from, to, focus);
	int s0 = 0;
	int s1 = length;
	int starts[TRAIL_VISIBLE_PARTS_MAX];
	int ends[TRAIL_VISIBLE_PARTS_MAX];
	int parts;

	if (length == 0)
		return count;
	if (!clip_axis(from.x, to.x - from.x, length, view->x_min, view->x_max, &s0, &s1) ||
		!clip_axis(from.y, to.y - from.y, length, view->y_min, view->y_max, &s0, &s1))
		return count;

	// Each part not hidden behind a nearer trail is its own strip. The pieces stay on the same
	// grid (next_near_boundary), so cutting a part out doesn't move the pieces either side of it
	if (!occluders)
		return add_strip(from, to, length, f, focus, s0, s1, out, count, max_columns);
	parts = trail_occluders_visible(occluders, from, to, s0, s1, starts, ends, TRAIL_VISIBLE_PARTS_MAX);
	for (int i = 0; i < parts; ++i)
		count = add_strip(from, to, length, f, focus, starts[i], ends[i], out, count, max_columns);
	return count;
}

int trail_clip_columns(const trail_column *columns, int count, const wall_clip_camera *camera,
					   int min_forward, trail_column *out)
{
	world_point strip[TRAIL_DRAW_COLUMNS_MAX];
	int kept = 0;
	int i = 0;

	while (i < count)
	{
		// A strip is a column that starts one (joined = 0) and the joined columns after it
		int end = i + 1;
		int n;
		wall_clip_run run;

		while (end < count && columns[end].joined)
			++end;
		n = end - i;
		for (int k = 0; k < n && k < TRAIL_DRAW_COLUMNS_MAX; ++k)
			strip[k] = columns[i + k].p; // copied first, so writing out can't overwrite what's unread

		run = wall_clip_strip(strip, n - 1, camera, min_forward);
		if (run.count > 0)
		{
			out[kept].p = run.start;
			out[kept++].joined = 0;
			for (int k = 1; k < run.count; ++k)
			{
				out[kept].p = strip[run.first + k];
				out[kept++].joined = 1;
			}
			out[kept].p = run.end;
			out[kept++].joined = 1;
		}
		i = end;
	}
	return kept;
}

int trail_build_columns(const trail *t, int head_gap, world_point focus, const world_rect *view,
						const trail_occluders *occluders, trail_column *out, int max_columns)
{
	world_point from, to;
	int count = 0;

	for (int i = trail_segment_count(t) - 1; i >= 0; --i) // newest first
		if (trail_get_segment(t, i, head_gap, &from, &to))
			count = add_segment(from, to, focus, view, occluders, out, count, max_columns);
	return count;
}
