#include "trail_occlusion.h"

// Products below stay inside an int: alongs are capped to +-LIMIT and acrosses to ACROSS_LIMIT,
// 8192 x 16384 = 2^27. Beyond those, nothing is in view anyway (trails are drawn to 6000)
#define ALONG_LIMIT 8192
#define ACROSS_LIMIT 16384

static int clamp(int v, int limit)
{
	return v > limit ? limit : v < -limit ? -limit : v;
}

// Rounded down and up (C's / rounds towards 0, which is up for negatives and down for positives).
// The hidden range is rounded inwards, so nothing visible is ever left out
static int div_floor(int n, int d)
{
	return n >= 0 ? n / d : -((-n + d - 1) / d);
}

static int div_ceil(int n, int d)
{
	return n >= 0 ? (n + d - 1) / d : -(-n / d);
}

// A segment running N/S/E/W, seen from the eye: which way it runs, how far across from the eye
// it is, and where it starts and ends along (from the eye)
typedef struct
{
	int runs_along_x; // 1: east-west (along x, across y); 0: north-south
	int across;		  // its distance across from the eye (signed: which side)
	int from_along;	  // its from end, along from the eye
	int to_along;
} line_view;

// Returns 0 for a diagonal or empty segment
static int view_of(world_point from, world_point to, world_point eye, line_view *v)
{
	if (from.y == to.y && from.x != to.x)
	{
		v->runs_along_x = 1;
		v->across = from.y - eye.y;
		v->from_along = from.x - eye.x;
		v->to_along = to.x - eye.x;
		return 1;
	}
	if (from.x == to.x && from.y != to.y)
	{
		v->runs_along_x = 0;
		v->across = from.x - eye.x;
		v->from_along = from.y - eye.y;
		v->to_along = to.y - eye.y;
		return 1;
	}
	return 0;
}

void trail_occluders_begin(trail_occluders *o, world_point eye)
{
	o->eye = eye;
	o->count = 0;
}

void trail_occluders_add(trail_occluders *o, const trail *t, int head_gap, const world_rect *view)
{
	world_point from;
	world_point to;

	// Newest first: if the list fills up, the oldest (usually furthest) are the ones left out
	for (int i = trail_segment_count(t) - 1; i >= 0 && o->count < TRAIL_OCCLUDERS_MAX; --i)
	{
		if (!trail_get_segment(t, i, head_gap, &from, &to))
			continue;
		// Wholly outside the view rectangle: it can't hide anything that's drawn
		if ((from.x < view->x_min && to.x < view->x_min) || (from.x > view->x_max && to.x > view->x_max) ||
			(from.y < view->y_min && to.y < view->y_min) || (from.y > view->y_max && to.y > view->y_max))
			continue;
		{
			line_view v;

			if (!view_of(from, to, o->eye, &v))
				continue; // diagonal: hides nothing
			o->along_x[o->count] = (unsigned char)v.runs_along_x;
			o->across[o->count] = v.across;
			o->along_min[o->count] = clamp(v.from_along < v.to_along ? v.from_along : v.to_along, ALONG_LIMIT);
			o->along_max[o->count] = clamp(v.from_along < v.to_along ? v.to_along : v.from_along, ALONG_LIMIT);
			o->count++;
		}
	}
}

// Takes the steps h0..h1 out of the parts list. Returns the new count; if a part would split
// and there's no room, it's left whole (drawn: never less than needed)
static int cut_out(int h0, int h1, int *starts, int *ends, int count, int max)
{
	for (int i = 0; i < count; ++i)
	{
		if (h1 <= starts[i] || h0 >= ends[i])
			continue; // doesn't touch this part
		if (h0 <= starts[i] && h1 >= ends[i])
		{
			// All of this part is hidden: remove it
			for (int k = i; k < count - 1; ++k)
			{
				starts[k] = starts[k + 1];
				ends[k] = ends[k + 1];
			}
			count--;
			i--;
		}
		else if (h0 <= starts[i])
			starts[i] = h1; // its start is hidden
		else if (h1 >= ends[i])
			ends[i] = h0; // its end is hidden
		else if (count < max)
		{
			// The middle is hidden: split it in two
			for (int k = count; k > i + 1; --k)
			{
				starts[k] = starts[k - 1];
				ends[k] = ends[k - 1];
			}
			starts[i + 1] = h1;
			ends[i + 1] = ends[i];
			ends[i] = h0;
			count++;
			i++;
		}
	}
	return count;
}

int trail_occluders_visible(const trail_occluders *o, world_point from, world_point to, int s0, int s1,
							int *starts, int *ends, int max)
{
	line_view far;
	int count = 1;
	int d2;
	// Along goes up with s (+1) or down (-1): the point at step s is from_along + direction * s
	int direction;

	starts[0] = s0;
	ends[0] = s1;
	if (max < 1)
		return 0;
	if (!view_of(from, to, o->eye, &far))
		return 1;
	direction = far.to_along > far.from_along ? 1 : -1;
	d2 = far.across < 0 ? -far.across : far.across;
	if (d2 > ACROSS_LIMIT)
		return 1;

	for (int i = 0; i < o->count && count > 0; ++i)
	{
		int d1;
		int t0;
		int t1;
		int h0;
		int h1;

		// Only a parallel segment, on the same side, nearer across (and not on the eye's own line)
		if (o->along_x[i] != far.runs_along_x || o->across[i] == 0 || (o->across[i] < 0) != (far.across < 0))
			continue;
		d1 = o->across[i] < 0 ? -o->across[i] : o->across[i];
		if (d1 >= d2)
			continue;

		// It hides the far line from A * d2 / d1 to B * d2 / d1 along (trail_occlusion.h), rounded
		// inwards. The only divisions here: two per pair of trails lined up like this
		t0 = div_ceil(o->along_min[i] * d2, d1);
		t1 = div_floor(o->along_max[i] * d2, d1);
		if (t0 >= t1)
			continue;

		// From along to steps on the far segment: s = (t - from_along) * direction
		h0 = direction > 0 ? t0 - far.from_along : far.from_along - t1;
		h1 = direction > 0 ? t1 - far.from_along : far.from_along - t0;
		count = cut_out(h0, h1, starts, ends, count, max);
	}
	return count;
}
