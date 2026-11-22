#include "screen_wall.h"
#include "../utils/fastmath.h" // ASR: shifts without libgcc's slow routines

int screen_wall_depth(const frustum *f, bool along_x, int line, int from, int to, int bottom, int top)
{
	// The axis across the wall (its plane's normal) and the one along it, as indexes of x, y, z
	const int across = along_x ? 1 : 0, along = along_x ? 0 : 1;
	const int eye[3] = {f->eye.x, f->eye.y, f->eye.z};
	// The screen's edges an eighth wider (as frustum_slopes: between rays, a rolled camera's edge
	// can be a little further out): a wall that only just covers the screen isn't taken
	const int tan_side = f->tan_side + (f->tan_side >> 3), tan_up = f->tan_up + (f->tan_up >> 3);
	const int to_line = line - eye[across]; // from the eye to the wall's plane
	int deepest = 0;

	if (to_line == 0)
		return -1; // the eye's in its plane: it's edge on
	for (int a = -1; a <= 1; a += 2)
		for (int b = -1; b <= 1; b += 2)
		{
			int d[3], dn, reach, depth;

			// The ray through that corner of the screen: forward, aside and up by the edges' tans
			// (x1024 x x1024, back down to x1024: as frustum_slopes)
			for (int k = 0; k < 3; ++k)
				d[k] = f->forward[k] + (ASR(a * tan_side * f->right[k] + b * tan_up * f->up[k], 10));
			dn = d[across];
			// Going towards the plane at all (not along it, not away)
			if ((dn > 0) != (to_line > 0) || dn == 0)
				return -1;
			/*
			 * Where it hits the plane, from the eye: d x (to_line / dn). Not divided: made positive
			 * (dn and to_line have the same sign) and compared cross-multiplied, so the hit along
			 * the wall is between its ends if (from - eye) x |dn| <= d_along x |to_line| <= (to - eye)
			 * x |dn|, and the same up it. (A wall near you: to_line under 2^10, the ends and heights
			 * from the eye under 2^13, d under 2^12: the products under 2^25)
			 */
			if (dn < 0)
				dn = -dn;
			reach = to_line < 0 ? -to_line : to_line;
			if (d[along] * reach < (from - eye[along]) * dn || d[along] * reach > (to - eye[along]) * dn)
				return -1; // past an end: that corner of the screen sees round it
			if (d[2] * reach < (bottom - eye[2]) * dn || d[2] * reach > (top - eye[2]) * dn)
				return -1; // under it or over it
			// Its depth along the view: (d . forward) / 1024 of the ray, times to_line / dn. The one
			// division here, four a wall that covers the screen
			depth = ((ASR(d[0] * f->forward[0] + d[1] * f->forward[1] + d[2] * f->forward[2], 10)) * reach) / dn;
			if (depth < SCREEN_WALL_NEAREST)
				return -1; // cut by the near plane there: not drawn, so it hides nothing (screen_wall.h)
			if (depth > deepest)
				deepest = depth;
		}
	return deepest;
}
