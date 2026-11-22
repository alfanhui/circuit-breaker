#include "trail_occlusion.h"

// Alongs are capped to +-ALONG_LIMIT, so a slope (along x 65536) stays inside an int: 2^13 x
// 2^16 = 2^29. Acrosses past ACROSS_LIMIT are ignored. Beyond those, nothing is in view anyway
// (trails are drawn to 6000)
#define ALONG_LIMIT 8192
#define ACROSS_LIMIT 16384
#define SLOPE_SHIFT 16 // slopes are x65536

// slope x d2, back from 16.16 to whole units: rounded down, or up. The product is 64 bits (a
// slope can be up to 2^29, d2 up to 2^14), which the SH-2 does in one instruction (dmuls.l), and
// the shift by 16 is one more (xtrct): no division, and no library call
static int scale_down(int slope, int d2)
{
	return (int)(((long long)slope * d2) >> SLOPE_SHIFT);
}

static int scale_up(int slope, int d2)
{
	return (int)(((long long)slope * d2 + ((1 << SLOPE_SHIFT) - 1)) >> SLOPE_SHIFT);
}

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

// Wholly outside the view rectangle: it can't hide anything that's drawn
static int outside(world_point from, world_point to, const world_rect *view)
{
	return (from.x < view->x_min && to.x < view->x_min) || (from.x > view->x_max && to.x > view->x_max) ||
		   (from.y < view->y_min && to.y < view->y_min) || (from.y > view->y_max && to.y > view->y_max);
}

// One segment onto the list, if it can hide anything and there's room
static void add_segment(trail_occluders *o, world_point from, world_point to, const world_rect *view)
{
	line_view v;
	int d1;
	int along_min;
	int along_max;

	if (o->count >= TRAIL_OCCLUDERS_MAX || outside(from, to, view))
		return;
	if (!view_of(from, to, o->eye, &v))
		return; // diagonal: hides nothing
	if (v.across == 0)
		return; // on the eye's own line: seen end on, it hides nothing (and d1 can't be 0)
	d1 = v.across < 0 ? -v.across : v.across;
	along_min = clamp(v.from_along < v.to_along ? v.from_along : v.to_along, ALONG_LIMIT);
	along_max = clamp(v.from_along < v.to_along ? v.to_along : v.from_along, ALONG_LIMIT);
	o->along_x[o->count] = (unsigned char)v.runs_along_x;
	o->across[o->count] = v.across;
	// The only divisions: two per segment listed, at most TRAIL_OCCLUDERS_MAX a frame
	o->slope_min[o->count] = div_ceil(along_min << SLOPE_SHIFT, d1);
	o->slope_max[o->count] = div_floor(along_max << SLOPE_SHIFT, d1);
	/*
	 * For far segments running across it (trail_occlusion.h): d1 / its ends, x65536, each side of
	 * the eye's line. On the + side the line of sight crosses it between 0 and c along, so only
	 * its part from max(A, 0) to B can hide anything there: none if B <= 0 (-1), and no near
	 * bound (0: to the view's edge) if A <= 0. The - side is the same, mirrored (-B to -A). The
	 * near bound's rounded down, the far one's up: what's hidden only comes out smaller. Up to
	 * four more divisions a segment listed (at most TRAIL_OCCLUDERS_MAX a frame)
	 */
	{
		const int n = o->count;
		const int d1_x = d1 << SLOPE_SHIFT; // (d1 under 2^14 here: under 2^30)

		if (d1 > ACROSS_LIMIT)
		{
			o->cross_k_far_pos[n] = o->cross_k_far_neg[n] = -1; // too far across to be worked out
			o->cross_k_near_pos[n] = o->cross_k_near_neg[n] = 0;
			o->spans_eye_line[n] = 0;
		}
		else
		{
			o->cross_k_near_pos[n] = along_min > 0 ? div_floor(d1_x, along_min) : 0;
			o->cross_k_far_pos[n] = along_max > 0 ? div_ceil(d1_x, along_max) : -1;
			o->cross_k_near_neg[n] = along_max < 0 ? div_floor(d1_x, -along_max) : 0;
			o->cross_k_far_neg[n] = along_min < 0 ? div_ceil(d1_x, -along_min) : -1;
			o->spans_eye_line[n] = (unsigned char)(along_min <= 0 && along_max >= 0);
		}
	}
	o->count++;
}

void trail_occluders_add(trail_occluders *o, const trail *t, int head_gap, const world_rect *view)
{
	world_point from;
	world_point to;

	// Newest first: if the list fills up, the oldest (usually furthest) are the ones left out
	for (int i = trail_segment_count(t) - 1; i >= 0 && o->count < TRAIL_OCCLUDERS_MAX; --i)
		if (trail_get_segment(t, i, head_gap, &from, &to))
			add_segment(o, from, to, view);
}

void trail_occluders_add_blocks(trail_occluders *o, const world_rect *blocks, int count, const world_rect *view)
{
	for (int i = 0; i < count; ++i)
	{
		const world_rect *b = &blocks[i];
		world_point from;
		world_point to;

		// The face on the eye's side across x (a north-south line), if it's off to one side
		if (o->eye.x < b->x_min || o->eye.x > b->x_max)
		{
			from.x = to.x = o->eye.x < b->x_min ? b->x_min : b->x_max;
			from.y = b->y_min;
			to.y = b->y_max;
			add_segment(o, from, to, view);
		}
		// And the face on its side across y (an east-west line)
		if (o->eye.y < b->y_min || o->eye.y > b->y_max)
		{
			from.y = to.y = o->eye.y < b->y_min ? b->y_min : b->y_max;
			from.x = b->x_min;
			to.x = b->x_max;
			add_segment(o, from, to, view);
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

		if (o->along_x[i] != far.runs_along_x)
		{
			// Running across it (trail_occlusion.h): hidden from |t| = c x d1 / B to c x d1 / A, and
			// only past it (|t| > d1). c is where the far one crosses its line, along from the eye
			const int c = far.across < 0 ? -far.across : far.across;
			const int k_near = far.across > 0 ? o->cross_k_near_pos[i] : o->cross_k_near_neg[i];
			const int k_far = far.across > 0 ? o->cross_k_far_pos[i] : o->cross_k_far_neg[i];
			int lo, hi;

			d1 = o->across[i] < 0 ? -o->across[i] : o->across[i];
			if (far.across == 0)
			{
				if (!o->spans_eye_line[i])
					continue; // straight ahead of the eye, and it's off to one side
				lo = d1 + 1;
				hi = ALONG_LIMIT << 2; // (nothing further is drawn)
			}
			else
			{
				if (k_far < 0)
					continue; // all of it's on the far one's other side of the eye's line
				lo = scale_up(k_far, c);
				if (lo < d1 + 1)
					lo = d1 + 1;
				hi = k_near == 0 ? ALONG_LIMIT << 2 : scale_down(k_near, c);
			}
			if (lo >= hi)
				continue;
			// In the far one's own along (its points' across from the near one's line), the near
			// one's side of the eye
			t0 = o->across[i] > 0 ? lo : -hi;
			t1 = o->across[i] > 0 ? hi : -lo;
			h0 = direction > 0 ? t0 - far.from_along : far.from_along - t1;
			h1 = direction > 0 ? t1 - far.from_along : far.from_along - t0;
			count = cut_out(h0, h1, starts, ends, count, max);
			continue;
		}
		// A parallel segment: on the same side, nearer across (none listed is on the eye's line)
		if ((o->across[i] < 0) != (far.across < 0))
			continue;
		d1 = o->across[i] < 0 ? -o->across[i] : o->across[i];
		if (d1 >= d2)
			continue;

		// It hides the far line from A * d2 / d1 to B * d2 / d1 along (trail_occlusion.h): its
		// slopes times d2, rounded inwards again. Multiplies, no division
		t0 = scale_up(o->slope_min[i], d2);
		t1 = scale_down(o->slope_max[i], d2);
		if (t0 >= t1)
			continue;

		// From along to steps on the far segment: s = (t - from_along) * direction
		h0 = direction > 0 ? t0 - far.from_along : far.from_along - t1;
		h1 = direction > 0 ? t1 - far.from_along : far.from_along - t0;
		count = cut_out(h0, h1, starts, ends, count, max);
	}
	return count;
}
