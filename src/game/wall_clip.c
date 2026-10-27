#include "wall_clip.h"

// Where a column is relative to the camera, in world units x1024. Comparisons work just as well
// scaled, and it saves two divisions per column: the SH-2 has no divide instruction, and no
// shift by 10 either, so even / 1024 is a library call there
typedef struct
{
	int forward; // in front of the camera (negative: behind)
	int right;	 // to its right (negative: left)
} view_pos;

#define SCALED(units) ((units) * 1024)

static view_pos view_pos_of(const wall_clip_camera *c, world_point p)
{
	view_pos v;
	int dx = p.x - c->eye.x; // up to ~30000 * 1024 per term: well inside an int
	int dy = p.y - c->eye.y;

	v.forward = dx * c->forward_x + dy * c->forward_y;
	// Right is forward turned 90 degrees clockwise seen from above: (forward_y, -forward_x)
	v.right = dx * c->forward_y - dy * c->forward_x;
	return v;
}

// Outside the 90 degree view: further to one side than it is in front (plus a margin)
static int left_of_view(view_pos v)
{
	return v.right < -(v.forward + SCALED(WALL_CLIP_FOV_MARGIN));
}

static int right_of_view(view_pos v)
{
	return v.right > v.forward + SCALED(WALL_CLIP_FOV_MARGIN);
}

static int piece_visible(view_pos a, view_pos b, int min_forward)
{
	min_forward = SCALED(min_forward);
	if (a.forward < min_forward && b.forward < min_forward)
		return 0; // too close (or behind) all along
	if (left_of_view(a) && left_of_view(b))
		return 0;
	if (right_of_view(a) && right_of_view(b))
		return 0;
	return 1;
}

// Slides `near` towards `far` until it's min_forward in front of the camera (if it isn't already)
static world_point slide_in_front(world_point near, world_point far, const wall_clip_camera *c, int min_forward)
{
	// Back to world units here (twice per strip, not per column): keeps the products below in range
	int fn = view_pos_of(c, near).forward / 1024;
	int ff = view_pos_of(c, far).forward / 1024;
	world_point p = near;

	if (fn >= min_forward || ff <= fn)
		return near;
	// Linear along the piece: the fraction (min_forward - fn) / (ff - fn) of the way to far
	p.x = (short)(near.x + (far.x - near.x) * (min_forward - fn) / (ff - fn));
	p.y = (short)(near.y + (far.y - near.y) * (min_forward - fn) / (ff - fn));
	return p;
}

wall_clip_run wall_clip_strip(const world_point *columns, int pieces,
							  const wall_clip_camera *camera, int min_forward)
{
	wall_clip_run run = {0, 0, {0, 0}, {0, 0}};
	int last = -1;
	view_pos left, right;

	if (pieces <= 0)
		return run;

	// Along a straight wall the visible pieces are one unbroken run: find its two ends.
	// Each column is placed relative to the camera once and shared by the two pieces either side
	right = view_pos_of(camera, columns[0]);
	for (int j = 0; j < pieces; ++j)
	{
		left = right;
		right = view_pos_of(camera, columns[j + 1]);
		if (piece_visible(left, right, min_forward))
		{
			if (last < 0)
				run.first = j;
			last = j;
		}
	}
	if (last < 0)
		return run;

	run.count = last - run.first + 1;
	run.start = slide_in_front(columns[run.first], columns[run.first + 1], camera, min_forward);
	run.end = slide_in_front(columns[last + 1], columns[last], camera, min_forward);
	return run;
}
