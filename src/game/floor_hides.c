#include "floor_hides.h"
#include "../utils/fastmath.h" // ASR: shifts without libgcc's slow routines

#define INV_SHIFT 20	 // the reciprocals are 2^20 / distance
#define EDGE_MIN 16		 // nearer an edge than this, that way hides nothing (no reciprocal)
#define REACH_MAX 16384	 // farther than this, a point isn't worked out (it'd overflow 32 bits): nothing hidden
#define RATIO_SHIFT 12	 // reach x reciprocal >> 12: the ratio x 256
#define RATIO_MAX 262144 // ratio x 256 past this: hidden only far below anything there is
#define MARGIN 8		 // a little lower than worked out, for the rounding: errs towards drawing

void floor_hides_none(floor_hides *f)
{
	f->on = 0;
}

// 2^20 / d, or 0 if d is too small to stand that close to an edge
static int reciprocal(int d)
{
	// (once a frame, four of them: the only divisions here)
	return d >= EDGE_MIN ? (1 << INV_SHIFT) / d : 0;
}

void floor_hides_begin(floor_hides *f, int eye_x, int eye_y, int eye_z, int x_min, int x_max, int y_min, int y_max,
					   int floor_z)
{
	f->on = eye_x > x_min && eye_x < x_max && eye_y > y_min && eye_y < y_max && eye_z > floor_z;
	if (!f->on)
		return;
	f->eye_x = eye_x;
	f->eye_y = eye_y;
	f->eye_z = eye_z;
	f->floor_z = floor_z;
	f->inv_x_min = reciprocal(eye_x - x_min);
	f->inv_x_max = reciprocal(x_max - eye_x);
	f->inv_y_min = reciprocal(eye_y - y_min);
	f->inv_y_max = reciprocal(y_max - eye_y);
}

/*
 * One way's ratio, x 256: how many times further than the floor's edge the point is, that way.
 * -1 when it can't be said (the eye at that edge, or the point too far): then nothing's hidden
 */
static int way_ratio(int d, int inv_low, int inv_high)
{
	int inv = d >= 0 ? inv_high : inv_low;

	if (d < 0)
		d = -d;
	if (d == 0)
		return 0; // straight above or below the eye that way: this way's edge isn't reached
	if (inv == 0 || d > REACH_MAX)
		return -1;
	return (int)((unsigned int)(d * inv) >> RATIO_SHIFT); // (d < 2^14, inv <= 2^16: under 2^30; both 0 or more:
	                                                    // a logical shift, no libgcc call)
}

int floor_hides_below(const floor_hides *f, int x, int y)
{
	int rx, ry, ratio, below;

	if (!f->on)
		return FLOOR_HIDES_NOTHING;
	rx = way_ratio(x - f->eye_x, f->inv_x_min, f->inv_x_max);
	ry = way_ratio(y - f->eye_y, f->inv_y_min, f->inv_y_max);
	if (rx < 0 || ry < 0)
		return FLOOR_HIDES_NOTHING;
	ratio = rx > ry ? rx : ry; // the edge the line of sight crosses first
	if (ratio > RATIO_MAX)
		return FLOOR_HIDES_NOTHING; // hidden only so far down there's nothing there
	// (H < 2^12 over any floor you stand on, ratio < 2^18: under 2^30)
	below = f->eye_z - (ASR((f->eye_z - f->floor_z) * ratio, 8)) - MARGIN;
	// Under the floor itself (ratio under 1) it's hidden only below the floor: not at its height,
	// where it's the floor's own surface (the unit test's first find)
	if (below > f->floor_z - 1)
		below = f->floor_z - 1;
	return below;
}

void floor_hides_line(int foot, int *from, int *to, int line_from, int line_to)
{
	// Below the foot at an end: it'd cut across the corner where foot and curve meet (floor_hides.h)
	if (line_from < foot || line_to < foot)
		return;
	// Higher on the whole than what's there: the larger sum (each end's under 2^15: no overflow)
	if (line_from + line_to > *from + *to)
	{
		*from = line_from;
		*to = line_to;
	}
}

int floor_hides_box(const floor_hides *f, int x_min, int x_max, int y_min, int y_max, int top)
{
	// Its top's four corners in the cone: the cone's convex, so all of it is
	return f->on && top <= floor_hides_below(f, x_min, y_min) && top <= floor_hides_below(f, x_max, y_min) &&
		   top <= floor_hides_below(f, x_max, y_max) && top <= floor_hides_below(f, x_min, y_max);
}
