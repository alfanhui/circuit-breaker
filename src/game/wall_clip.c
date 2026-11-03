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

// Outside the view: further to one side than the view reaches at that distance in front (plus a
// margin). The view reaches `width` sixteenths of the distance in front (wall_clip.h), so both
// sides are compared x16 rather than dividing. Largest products: ~4e7 x 16 and ~4e7 x 32, well
// inside an int
// extra16: the view's reach further aside at any distance (wall_clip.h), x16
static int left_of_view(view_pos v, int width, int extra16)
{
	return v.right * 16 < -(v.forward * width + SCALED(WALL_CLIP_FOV_MARGIN * 16 + extra16));
}

static int right_of_view(view_pos v, int width, int extra16)
{
	return v.right * 16 > v.forward * width + SCALED(WALL_CLIP_FOV_MARGIN * 16 + extra16);
}

static int piece_visible(view_pos a, view_pos b, int min_forward, int width, int extra16)
{
	min_forward = SCALED(min_forward);
	if (a.forward < min_forward && b.forward < min_forward)
		return 0; // too close (or behind) all along
	if (left_of_view(a, width, extra16) && left_of_view(b, width, extra16))
		return 0;
	if (right_of_view(a, width, extra16) && right_of_view(b, width, extra16))
		return 0;
	return 1;
}

// Slides `near` towards `far` until it's min_forward in front of the camera (if it isn't already)
static world_point slide_in_front(world_point near, world_point far, const wall_clip_camera *c, int min_forward)
{
	int fn, ff;
	world_point p = near;

	// Most ends are in front already: seen in the scaled sums, with no division (it gives the
	// same answer as the test below: an int truncated to whole units is min_forward or more just
	// when it's min_forward x 1024 or more). Divided first, every strip cost four divisions here
	// for nothing: with the cone's, the grey box's sides were a seventh of the frame
	if (view_pos_of(c, near).forward >= SCALED(min_forward))
		return near;
	// Back to world units here (twice per strip, not per column): keeps the products below in range
	fn = view_pos_of(c, near).forward / 1024;
	ff = view_pos_of(c, far).forward / 1024;

	if (fn >= min_forward || ff <= fn)
		return near;
	// Linear along the piece: the fraction (min_forward - fn) / (ff - fn) of the way to far
	p.x = (short)(near.x + (far.x - near.x) * (min_forward - fn) / (ff - fn));
	p.y = (short)(near.y + (far.y - near.y) * (min_forward - fn) / (ff - fn));
	return p;
}

// Slides `near` towards `far` until it's within the cone: |right| x 16 <= reach16 x forward. How far
// inside the cone's edge a point is (g, below) changes in a straight line along the piece, so
// where it's 0 is one division away. Sets *outside when no part of the piece is in the cone
static world_point slide_into_cone(world_point near, world_point far, const wall_clip_camera *c, int reach16,
								   int *outside)
{
	const view_pos vn = view_pos_of(c, near);
	view_pos vf;
	int fn, ff, rn, rf, side, gn, gf;
	world_point p = near;

	*outside = 0;
	// Most ends are well inside the cone already: seen in the scaled sums (x1024), with no
	// division. Its test below is in whole units, each truncated (up to 1 off): reach16 + 16 more
	// than nothing covers that, so this only ever returns where the test below would too
	// (products: reach16 x ~3e7 and 16 x ~3e7, under 2^31)
	side = vn.right <= -1024 ? -1 : 1; // as rn < 0, below
	if (reach16 * vn.forward + SCALED(c->side_extra16) - 16 * side * vn.right >= SCALED(reach16 + 17))
		return near;
	// World units (one division each by 1024: twice an end), so the products stay inside an int
	vf = view_pos_of(c, far);
	fn = vn.forward / 1024;
	ff = vf.forward / 1024;
	rn = vn.right / 1024;
	rf = vf.right / 1024;
	// Which side it's out on: that's the edge it comes in over
	side = rn < 0 ? -1 : 1;
	// How far inside the cone's edge each end is, x16 (negative: outside)
	gn = reach16 * fn + c->side_extra16 - 16 * side * rn;
	gf = reach16 * ff + c->side_extra16 - 16 * side * rf;

	if (gn >= 0 || gf <= gn)
		return near; // already inside, or the far end's no further in: nothing to slide to
	if (gf < 0)
	{
		*outside = 1; // never in it along this piece (one only just in view): as far as it goes
		return far;
	}
	// The fraction -gn / (gf - gn) of the way to far. Products up to ~250 x 30000: inside an int
	p.x = (short)(near.x + (far.x - near.x) * -gn / (gf - gn));
	p.y = (short)(near.y + (far.y - near.y) * -gn / (gf - gn));
	return p;
}

wall_clip_run wall_clip_strip_in_cone(const world_point *columns, int pieces, const wall_clip_camera *camera,
									  int min_forward, int reach16)
{
	wall_clip_run run = wall_clip_strip(columns, pieces, camera, min_forward);
	int start_out, end_out;

	if (run.count == 0)
		return run;
	// Slid in front already; now in the cone, towards the piece's other column (only ever further
	// along, so it stays in front). An end piece wholly outside it shrinks to nothing; a run of
	// just that piece is dropped (its two ends would swap over)
	run.start = slide_into_cone(run.start, columns[run.first + 1], camera, reach16, &start_out);
	run.end = slide_into_cone(run.end, columns[run.first + run.count - 1], camera, reach16, &end_out);
	if (run.count == 1 && (start_out || end_out))
		run.count = 0;
	return run;
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
		if (piece_visible(left, right, min_forward, camera->view_width, camera->side_extra16))
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

int wall_clip_object_visible(const wall_clip_camera *camera, world_point p, int radius, int min_forward)
{
	view_pos v = view_pos_of(camera, p);

	// Its nearest point must be min_forward in front: its centre, radius + min_forward
	if (v.forward < SCALED(radius + min_forward))
		return 0;
	// Off to one side: its centre is more than radius beyond the view's edge. Moving the centre
	// radius towards the middle and testing that is the same, without a division
	v.right += v.right > 0 ? -SCALED(radius) : SCALED(radius);
	return !left_of_view(v, camera->view_width, camera->side_extra16) &&
		   !right_of_view(v, camera->view_width, camera->side_extra16);
}
