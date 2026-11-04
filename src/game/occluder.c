#include "occluder.h"

void occluder_set(occluder *o, frustum_point eye, bool along_x, int line, int from, int to, int bottom, int top)
{
	// How far the eye is from the rectangle's line, and which side of it
	const int off = (along_x ? eye.y : eye.x) - line;
	const int middle = (from + to) >> 1;
	frustum_point corner[4], inside;
	frustum_plane *own;

	o->on = false;
	o->sides = 0;
	o->f.eye = eye;
	o->f.count = 0;
	if (off < OCCLUDER_NEAREST && off > -OCCLUDER_NEAREST)
		return; // in it, or near enough to be: see OCCLUDER_NEAREST
	// Its corners round it: (from, bottom), (from, top), (to, top), (to, bottom); and its middle,
	// which is inside all four of the pyramid's sides
	for (int k = 0; k < 4; ++k)
	{
		const int along = k < 2 ? from : to;

		corner[k].x = along_x ? along : line;
		corner[k].y = along_x ? line : along;
		corner[k].z = (k == 1 || k == 2) ? top : bottom;
	}
	inside.x = along_x ? middle : line;
	inside.y = along_x ? line : middle;
	inside.z = (bottom + top) >> 1;
	// The four sides: each edge and the eye. frustum_add_edge points each plane at `inside`
	for (int k = 0; k < 4; ++k)
	{
		const unsigned bit = frustum_add_edge(&o->f, corner[k], corner[(k + 1) & 3], inside);

		if (bit == 0)
			return; // no plane (an edge in line with the eye): leave it off
		o->sides |= bit;
	}
	// The rectangle's own plane, its inside the far side from the eye. Lined up with an axis, so
	// it's set straight: the normal one unit along x or y, and the plane `off` away from the eye.
	// A point p is inside when normal . (p - eye) >= offset (frustum.h), and the eye's at -off
	// from the line along that axis: so the normal is -sign(off), the offset |off|
	own = &o->f.planes[o->f.count];
	own->x = own->y = own->z = 0;
	if (along_x)
		own->y = off > 0 ? -FRUSTUM_ONE : FRUSTUM_ONE;
	else
		own->x = off > 0 ? -FRUSTUM_ONE : FRUSTUM_ONE;
	own->offset = (off > 0 ? off : -off) * FRUSTUM_ONE;
	o->own = o->f.count;
	++o->f.count;
	o->on = true;
}

/*
 * Wholly inside every plane: tested one plane at a time, stopping at the first the box isn't wholly
 * inside. The rectangle's own plane first (o->own): a box on the eye's side of the wall, or across
 * its line, fails there, at one plane's sums, and most do. frustum_classify_box would go on through
 * all five (it stops only when a box is wholly outside one). A plane's nearest corner to it is the
 * centre's distance less |normal x| x half x + ... (frustum.h)
 */
bool occluder_hides(const occluder *o, frustum_point centre, frustum_point half)
{
	if (!o->on)
		return false;
	for (int k = 0; k < 5; ++k)
	{
		const int i = k == 0 ? o->own : k - 1; // the own plane, then the four sides (planes 0 .. 3)
		const frustum_plane *pl = &o->f.planes[i];
		const int reach = (pl->x < 0 ? -pl->x : pl->x) * half.x + (pl->y < 0 ? -pl->y : pl->y) * half.y +
						  (pl->z < 0 ? -pl->z : pl->z) * half.z;

		if (frustum_distance(&o->f, i, centre) < reach)
			return false; // some of it's outside this plane: not all in the shadow
	}
	return true;
}

bool occluder_shows(const occluder *o, frustum_point centre, frustum_point half)
{
	return !o->on || frustum_classify_box(&o->f, centre, half, o->sides) != FRUSTUM_OUTSIDE;
}
