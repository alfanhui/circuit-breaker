#include "surround.h"

static void set_piece(surround_piece *p, int side, bool rim, int x_min, int y_min, int x_max, int y_max)
{
	p->area.x_min = (short)x_min;
	p->area.y_min = (short)y_min;
	p->area.x_max = (short)x_max;
	p->area.y_max = (short)y_max;
	p->side = (unsigned char)side;
	p->rim = rim;
}

void surround_pieces(int half, surround_piece *out)
{
	const int rim = half + SURROUND_RIM; // the rim's outer edge
	const int far = SURROUND_EXTENT;

	// North is -y, west is +x (world.h). North and south run the whole width, corners and all;
	// east and west fit between them
	set_piece(&out[0], SURROUND_NORTH, true, -rim, -rim, rim, -half);
	set_piece(&out[1], SURROUND_NORTH, false, -far, -far, far, -rim);
	set_piece(&out[2], SURROUND_SOUTH, true, -rim, half, rim, rim);
	set_piece(&out[3], SURROUND_SOUTH, false, -far, rim, far, far);
	set_piece(&out[4], SURROUND_EAST, true, -rim, -half, -half, half);
	set_piece(&out[5], SURROUND_EAST, false, -far, -rim, -rim, rim);
	set_piece(&out[6], SURROUND_WEST, true, half, -half, rim, half);
	set_piece(&out[7], SURROUND_WEST, false, rim, -rim, far, rim);
}

/*
 * One step of Sutherland-Hodgman: keeps the part of polygon `in` (n corners) on the kept side of
 * the line x = bound (axis 0) or y = bound (axis 1). keep_above: keep x (or y) >= bound, else <=.
 * Walking round the edges a -> b, each edge adds:
 *   a, if a is kept
 *   the point where it crosses the line, if a and b are on opposite sides
 * Returns the new number of corners: at most one more than before.
 */
static int clip_side(const world_point *in, int n, world_point *out, int axis, int bound, bool keep_above)
{
	int m = 0;

	for (int i = 0; i < n; ++i)
	{
		world_point a = in[i];
		world_point b = in[i + 1 < n ? i + 1 : 0];
		// How far each is past the line, + on the kept side
		int da = (axis == 0 ? a.x : a.y) - bound;
		int db = (axis == 0 ? b.x : b.y) - bound;

		if (!keep_above)
		{
			da = -da;
			db = -db;
		}
		if (da >= 0)
			out[m++] = a;
		if ((da >= 0) != (db >= 0))
		{
			// Where it crosses: da / (da - db) of the way from a to b. The other coordinate's
			// step (up to 64000) times da (up to 64000) would pass 32 bits, so both sides of the
			// fraction are shifted down 2 first (16 million times 64000 fits). The one division
			// in the clipping, once per edge this line cuts
			int step = axis == 0 ? b.y - a.y : b.x - a.x;
			int span = (da - db) >> 2; // da and db differ in sign: never 0 unless they're within 4
			int other = axis == 0 ? a.y : a.x;
			world_point p;

			if (span != 0)
				other += step * (da >> 2) / span;
			if (axis == 0)
			{
				p.x = (short)bound; // exactly on the line, so neighbouring pieces meet with no gap
				p.y = (short)other;
			}
			else
			{
				p.x = (short)other;
				p.y = (short)bound;
			}
			out[m++] = p;
		}
	}
	return m;
}

int surround_clip(const world_rect *r, const world_point quad[4], world_point *out)
{
	// Two buffers in turn: each side reads one and writes the other
	world_point a[SURROUND_CLIP_MAX];
	world_point b[SURROUND_CLIP_MAX];
	int n = 4;
	int x_lo = quad[0].x, x_hi = quad[0].x, y_lo = quad[0].y, y_hi = quad[0].y;

	for (int i = 1; i < 4; ++i)
	{
		x_lo = quad[i].x < x_lo ? quad[i].x : x_lo;
		x_hi = quad[i].x > x_hi ? quad[i].x : x_hi;
		y_lo = quad[i].y < y_lo ? quad[i].y : y_lo;
		y_hi = quad[i].y > y_hi ? quad[i].y : y_hi;
	}
	// Most pieces are nowhere near the screen: a quick test against the quad's box first
	if (x_hi < r->x_min || x_lo > r->x_max || y_hi < r->y_min || y_lo > r->y_max)
		return 0;

	for (int i = 0; i < 4; ++i)
		a[i] = quad[i];
	n = clip_side(a, n, b, 0, r->x_min, true);
	n = clip_side(b, n, a, 0, r->x_max, false);
	n = clip_side(a, n, b, 1, r->y_min, true);
	n = clip_side(b, n, a, 1, r->y_max, false);
	if (n < 3)
		return 0; // only touching: an edge or a point
	for (int i = 0; i < n; ++i)
		out[i] = a[i];
	return n;
}

bool surround_in_front(int side, world_point eye, int half)
{
	switch (side)
	{
	case SURROUND_NORTH:
		return eye.y < -half;
	case SURROUND_SOUTH:
		return eye.y > half;
	case SURROUND_EAST:
		return eye.x < -half;
	default:
		return eye.x > half;
	}
}
