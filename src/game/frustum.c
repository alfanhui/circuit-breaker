#include "frustum.h"

int frustum_isqrt(unsigned int v)
{
	// Bit by bit, from the top: no division (the SH-2 has no divide instruction)
	unsigned int root = 0, bit = 1u << 30;

	while (bit > v)
		bit >>= 2;
	while (bit != 0)
	{
		if (v >= root + bit)
		{
			v -= root + bit;
			root = (root >> 1) + bit;
		}
		else
			root >>= 1;
		bit >>= 2;
	}
	return (int)root;
}

static int size_of(int v)
{
	return v < 0 ? -v : v;
}

/*
 * Sets plane i's normal to (x, y, z) made FRUSTUM_ONE long, and its offset. The parts are brought
 * under 2^14 first (halved together: the direction's kept), so their squares' sum fits 32 bits.
 * Three divisions: once a frame for each plane, never per polygon. False if there's no direction
 */
static bool set_plane(frustum_plane *p, int x, int y, int z, int offset)
{
	int length;

	while (size_of(x) >= (1 << 14) || size_of(y) >= (1 << 14) || size_of(z) >= (1 << 14))
	{
		x >>= 1;
		y >>= 1;
		z >>= 1;
	}
	length = frustum_isqrt((unsigned int)(x * x + y * y + z * z));
	if (length == 0)
		return false;
	p->x = x * FRUSTUM_ONE / length;
	p->y = y * FRUSTUM_ONE / length;
	p->z = z * FRUSTUM_ONE / length;
	p->offset = offset * FRUSTUM_ONE;
	return true;
}

void frustum_set_camera(frustum *f, frustum_point eye, const int forward[3], const int right[3], const int up[3],
						int tan_side, int tan_up, int near)
{
	f->eye = eye;
	for (int k = 0; k < 3; ++k)
	{
		f->forward[k] = forward[k];
		f->right[k] = right[k];
		f->up[k] = up[k];
	}
	// Near: in front of the eye by `near`
	set_plane(&f->planes[0], forward[0], forward[1], forward[2], near);
	/*
	 * The screen's edges. A point p, v = p - eye, is left of the right edge when it's no further
	 * right than tan_side times its distance in front: right . v <= tan_side x forward . v, so
	 * (tan_side x forward - right) . v >= 0. The others the same way. Both sides x1024 x1024
	 */
	for (int k = 0; k < 8; ++k)
	{
		const int *aside = (k & 3) < 2 ? right : up;
		// The screen's (k 0 .. 3), then the guard band's, wider (k 4 .. 7)
		const int tan = (((k & 3) < 2 ? tan_side : tan_up) * (k < 4 ? 16 : FRUSTUM_GUARD_X16)) >> 4;
		const int sign = (k & 1) ? 1 : -1; // right then left, top then bottom

		set_plane(&f->planes[1 + k], tan * forward[0] + sign * FRUSTUM_ONE * aside[0],
				  tan * forward[1] + sign * FRUSTUM_ONE * aside[1], tan * forward[2] + sign * FRUSTUM_ONE * aside[2], 0);
	}
	f->count = FRUSTUM_PORTAL_FIRST;
}

// How far p is inside plane pl, x FRUSTUM_ONE: three multiplies (the SH-2's are quick)
static inline int distance_to(const frustum_plane *pl, const frustum_point *eye, const frustum_point *p)
{
	return pl->x * (p->x - eye->x) + pl->y * (p->y - eye->y) + pl->z * (p->z - eye->z) - pl->offset;
}

int frustum_distance(const frustum *f, int i, frustum_point p)
{
	return distance_to(&f->planes[i], &f->eye, &p);
}

unsigned frustum_add_edge(frustum *f, frustum_point a, frustum_point b, frustum_point inside)
{
	// The normal's across both lines from the eye: (a - eye) x (b - eye). Each part's under 2^14
	// (a few thousand from the eye), so the products stay inside 32 bits
	const int ax = a.x - f->eye.x, ay = a.y - f->eye.y, az = a.z - f->eye.z;
	const int bx = b.x - f->eye.x, by = b.y - f->eye.y, bz = b.z - f->eye.z;
	frustum_plane *p;

	if (f->count >= FRUSTUM_PLANES_MAX)
		return 0;
	p = &f->planes[f->count];
	if (!set_plane(p, ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx, 0))
		return 0; // the eye's on the line: no plane
	if (frustum_distance(f, f->count, inside) < 0)
	{
		p->x = -p->x;
		p->y = -p->y;
		p->z = -p->z;
	}
	return 1u << f->count++;
}

/*
 * The planes' bits are walked with a bit moved on by one each time (bit <<= 1), not made as
 * 1 << i: the SH-2 shifts only by 1, 2, 8 or 16 in one instruction, and GCC makes any other shift
 * a library call. Per plane per corner, those calls were most of the boxes' time
 */
unsigned frustum_classify(const frustum *f, const frustum_point *corners, int n, unsigned planes)
{
	unsigned cut = 0, bit = 1;

	for (int i = 0; i < f->count; ++i, bit <<= 1)
	{
		int outside = 0;

		if (!(planes & bit))
			continue;
		for (int k = 0; k < n; ++k)
			if (distance_to(&f->planes[i], &f->eye, &corners[k]) < 0)
				++outside;
		if (outside == n)
			return FRUSTUM_OUTSIDE;
		if (outside > 0)
			cut |= bit;
	}
	return cut;
}

static int size_times(int a, int b)
{
	return (a < 0 ? -a : a) * b;
}

unsigned frustum_classify_box(const frustum *f, frustum_point centre, frustum_point half, unsigned planes)
{
	unsigned cut = 0, bit = 1;

	for (int i = 0; i < f->count; ++i, bit <<= 1)
	{
		const frustum_plane *pl = &f->planes[i];
		int d, reach;

		if (!(planes & bit))
			continue;
		d = distance_to(pl, &f->eye, &centre);
		reach = size_times(pl->x, half.x) + size_times(pl->y, half.y) + size_times(pl->z, half.z);
		if (d + reach < 0)
			return FRUSTUM_OUTSIDE; // even its furthest-in corner's outside
		if (d - reach < 0)
			cut |= bit; // its furthest-out corner's outside: it crosses
	}
	return cut;
}

/*
 * The corner where the edge from a (inside, da >= 0) to b (outside, db < 0) crosses the plane:
 * a + (b - a) x da / (da - db). The share's worked out in 16 bits: da and da - db are halved
 * together until da - db is under 2^15, so da x 2^16 fits 32 bits (da <= da - db), and one 32-bit
 * division (64-bit is a library call of hundreds of cycles on the SH-2). Then (b - a) x share in
 * 64 bits (one dmuls.l) and >> 16, a shift the SH-2 has (>> 14 was a library call). Off by under a
 * unit for an edge a few thousand long
 */
static frustum_point crossing(frustum_point a, frustum_point b, int da, int db)
{
	int span = da - db;
	int share;
	frustum_point p;

	while (span >= (1 << 15))
	{
		span >>= 1;
		da >>= 1;
	}
	share = span == 0 ? 0 : (da << 16) / span;
	p.x = a.x + (int)(((long long)(b.x - a.x) * share) >> 16);
	p.y = a.y + (int)(((long long)(b.y - a.y) * share) >> 16);
	p.z = a.z + (int)(((long long)(b.z - a.z) * share) >> 16);
	return p;
}

int frustum_clip(const frustum *f, unsigned planes, const frustum_point *in, int n, frustum_point *out)
{
	frustum_point buffer[2][FRUSTUM_POLY_MAX];
	const frustum_point *from = in;
	int count = n;
	int which = 0;

	unsigned bit = 1;

	for (int i = 0; i < f->count && count >= 3; ++i, bit <<= 1)
	{
		frustum_point *to;
		int kept = 0;
		int d_prev;

		if (!(planes & bit))
			continue;
		to = buffer[which];
		d_prev = distance_to(&f->planes[i], &f->eye, &from[count - 1]);
		// Each edge, from the corner before to this one: this corner's kept if inside, and a new
		// one added where the edge crosses the plane
		for (int k = 0; k < count; ++k)
		{
			const frustum_point prev = from[k == 0 ? count - 1 : k - 1];
			const int d = distance_to(&f->planes[i], &f->eye, &from[k]);

			if ((d >= 0) != (d_prev >= 0))
				to[kept++] = d >= 0 ? crossing(from[k], prev, d, d_prev) : crossing(prev, from[k], d_prev, d);
			if (d >= 0)
				to[kept++] = from[k];
			d_prev = d;
		}
		from = to;
		count = kept;
		which ^= 1;
	}
	if (count < 3)
		return 0;
	for (int k = 0; k < count; ++k)
		out[k] = from[k];
	return count;
}

void frustum_reflect(const frustum *in, int axis, int mirror, frustum *out)
{
	*out = *in;
	if (axis == 0)
	{
		out->eye.x = 2 * mirror - in->eye.x;
		out->forward[0] = -in->forward[0];
		out->right[0] = -in->right[0];
		out->up[0] = -in->up[0];
		for (int i = 0; i < in->count; ++i)
			out->planes[i].x = -in->planes[i].x;
	}
	else
	{
		out->eye.y = 2 * mirror - in->eye.y;
		out->forward[1] = -in->forward[1];
		out->right[1] = -in->right[1];
		out->up[1] = -in->up[1];
		for (int i = 0; i < in->count; ++i)
			out->planes[i].y = -in->planes[i].y;
	}
}
