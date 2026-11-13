#include "skyline.h"

void skyline_begin(skyline *s, int x, int y, int z, int forward_x, int forward_y)
{
	s->x = x;
	s->y = y;
	s->z = z;
	s->fx = forward_x;
	s->fy = forward_y;
	s->count = 0;
	s->worst = 0;
}

/*
 * A box's silhouette from the eye: the two of its corners furthest apart as the eye sees them,
 * from the eye. Which two depends only on where the eye is round the box:
 *   beside it (within its span along one axis): the two corners of the side facing the eye
 *   off a corner: the two corners next to the nearest one (the box's diagonal, seen across)
 * Ordered so b is to the left of a (a x b > 0). false if the eye's over the box: no silhouette
 */
static bool silhouette(const skyline *s, const skyline_box *b, int *ax, int *ay, int *bx, int *by)
{
	const int side_x = s->x < b->x_min ? -1 : s->x > b->x_max ? 1 : 0;
	const int side_y = s->y < b->y_min ? -1 : s->y > b->y_max ? 1 : 0;
	const int near_x = (side_x < 0 ? b->x_min : b->x_max) - s->x, far_x = (side_x < 0 ? b->x_max : b->x_min) - s->x;
	const int near_y = (side_y < 0 ? b->y_min : b->y_max) - s->y, far_y = (side_y < 0 ? b->y_max : b->y_min) - s->y;
	int px, py, qx, qy;

	if (side_x == 0 && side_y == 0)
		return false;
	if (side_y == 0)
	{
		// East or west of it: the side facing you, end to end along y
		px = qx = near_x;
		py = b->y_min - s->y;
		qy = b->y_max - s->y;
	}
	else if (side_x == 0)
	{
		py = qy = near_y;
		px = b->x_min - s->x;
		qx = b->x_max - s->x;
	}
	else
	{
		px = far_x;
		py = near_y;
		qx = near_x;
		qy = far_y;
	}
	// Which is to the left of which: by their cross product's sign
	if (px * qy - py * qx > 0)
	{
		*ax = px;
		*ay = py;
		*bx = qx;
		*by = qy;
	}
	else
	{
		*ax = qx;
		*ay = qy;
		*bx = px;
		*by = py;
	}
	return true;
}

void skyline_offer(skyline *s, const skyline_box *b)
{
	skyline_hider h;
	int dx, dy, slot;

	if (b->top < s->z)
		return; // you see over it: what's behind shows above it
	// Its nearest point to the eye: how far along x and along y (0 within its span)
	dx = s->x < b->x_min ? b->x_min - s->x : s->x > b->x_max ? s->x - b->x_max : 0;
	dy = s->y < b->y_min ? b->y_min - s->y : s->y > b->y_max ? s->y - b->y_max : 0;
	if (dx == 0 && dy == 0)
		return; // the eye's over it: no silhouette
	// How good a hider: near is good, and so is tall (a low tower next to you hides nothing that
	// matters: everything behind it stands over it). So: how far, less how far it's over your eye
	h.reach = (dx > dy ? dx : dy) - (b->top - s->z);
	slot = s->count;
	if (slot == SKYLINE_HIDERS)
	{
		// Full: only in place of the worst kept, if this one's better
		slot = s->worst;
		if (s->hiders[slot].reach <= h.reach)
			return;
	}
	// In front: its middle, from the eye, along the way you look (twice it, to save the halving)
	if ((b->x_min + b->x_max - 2 * s->x) * s->fx + (b->y_min + b->y_max - 2 * s->y) * s->fy <= 0)
		return;
	silhouette(s, b, &h.ax, &h.ay, &h.bx, &h.by);
	h.top = b->top;
	{
		// Its furthest corner: the further end along x and along y, and no less than how far
		// that is in a straight line (the longer plus half the shorter: never under, no square root)
		const int to_min_x = s->x - b->x_min, to_max_x = b->x_max - s->x;
		const int to_min_y = s->y - b->y_min, to_max_y = b->y_max - s->y;
		const int far_x = to_min_x > to_max_x ? to_min_x : to_max_x, far_y = to_min_y > to_max_y ? to_min_y : to_max_y;

		h.far = far_x > far_y ? far_x + (far_y >> 1) : far_y + (far_x >> 1);
	}
	s->hiders[slot] = h;
	if (s->count < SKYLINE_HIDERS)
		++s->count;
	if (s->count == SKYLINE_HIDERS)
	{
		// Which is the worst now: the next offer's only compared with that one
		s->worst = 0;
		for (int k = 1; k < SKYLINE_HIDERS; ++k)
			if (s->hiders[k].reach > s->hiders[s->worst].reach)
				s->worst = k;
	}
}

// Whether the point (from the eye) is in h's shadow on the plan: inside its wedge, beyond its chord
static bool behind(const skyline_hider *h, int x, int y)
{
	// Left of a, right of b: inside the wedge
	if (h->ax * y - h->ay * x < 0 || x * h->by - y * h->bx < 0)
		return false;
	// Beyond the line from a to b: the eye (0, 0) is on its other side, where this is positive
	return (h->bx - h->ax) * (y - h->ay) - (h->by - h->ay) * (x - h->ax) < 0;
}

static bool hides(const skyline *s, const skyline_box *b, bool steeper)
{
	const int x0 = b->x_min - s->x, x1 = b->x_max - s->x, y0 = b->y_min - s->y, y1 = b->y_max - s->y;
	// The box's nearest point to the eye: how far, at the least (the longer way along x or y)
	const int dx = x0 > 0 ? x0 : x1 < 0 ? -x1 : 0, dy = y0 > 0 ? y0 : y1 < 0 ? -y1 : 0;
	const int nearest = dx > dy ? dx : dy;
	const int rise = b->top - s->z; // how far its top's over the eye

	for (int i = 0; i < s->count; ++i)
	{
		const skyline_hider *h = &s->hiders[i];

		// Under the hider's top: level with it or lower, or (steeper) seen at a lower angle.
		// Rises are under 2^14 and distances 2^16: the products stay inside 32 bits
		if (h->top < b->top && !(steeper && rise * h->far <= (h->top - s->z) * nearest))
			continue;
		if (behind(h, x0, y0) && behind(h, x1, y0) && behind(h, x1, y1) && behind(h, x0, y1))
			return true;
	}
	return false;
}

bool skyline_trim(const skyline *s, bool along_x, int line, int *from, int *to, int top)
{
	const int fixed = line - (along_x ? s->y : s->x); // the side's line, from the eye
	const int eye_along = along_x ? s->x : s->y;

	for (int i = 0; i < s->count; ++i)
	{
		const skyline_hider *h = &s->hiders[i];
		// The side's two ends, from the eye, along its own axis
		const int u0 = *from - eye_along, u1 = *to - eye_along;
		bool hidden0, hidden1;

		if (h->top < top)
			continue; // the side stands over it
		hidden0 = along_x ? behind(h, u0, fixed) : behind(h, fixed, u0);
		hidden1 = along_x ? behind(h, u1, fixed) : behind(h, fixed, u1);
		if (hidden0 && hidden1)
			return false;
		if (hidden0 || hidden1)
		{
			// From the hidden end towards the one that shows: `in` stays in the shadow, `out` out
			int in = hidden0 ? u0 : u1, out = hidden0 ? u1 : u0;

			for (int k = 0; k < SKYLINE_TRIM_STEPS; ++k)
			{
				const int mid = (in + out) >> 1;

				if (along_x ? behind(h, mid, fixed) : behind(h, fixed, mid))
					in = mid;
				else
					out = mid;
			}
			if (hidden0)
				*from = in + eye_along;
			else
				*to = in + eye_along;
		}
	}
	return *from < *to;
}

bool skyline_hides(const skyline *s, const skyline_box *b)
{
	return hides(s, b, true);
}

bool skyline_hides_level(const skyline *s, const skyline_box *b)
{
	return hides(s, b, false);
}
