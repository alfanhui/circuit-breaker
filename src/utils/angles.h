#pragma once
#include <jo/jo.h>

/*
 * What SGL gave us for angles and the 3D matrix, now from jo-engine 2026 (which has no SGL).
 *
 * Angles are jo_angle, as SGL's ANGLE was: 0x10000 is a whole turn, so 90 degrees is 0x4000
 * (JO_DEG_TO_ANGLE). jo_sin_angle and jo_cos_angle look up the same kind of table slSin and
 * slCos did (4096 steps a turn) and return 16.16 fixed point.
 */

/*
 * tan, as slTan was: 16.16. SGL kept a table of it; jo has only jo_tan(degrees), whole degrees,
 * too coarse for a field of view that eases while boosting. So sin / cos, with jo_fixed_div:
 * the SH-2's own division unit (~39 cycles, CLAUDE.md), not the software __sdivsi3. Still a
 * division, so once per frame or per object, never per vertex.
 * At 90 degrees cos is 0: the largest number instead (SGL's table ended in a large value too).
 */
static __jo_force_inline jo_fixed angle_tan(jo_angle angle)
{
	jo_fixed s;
	jo_fixed c;

	jo_sincos_angle(angle, &s, &c);
	if (c == 0)
		return s >= 0 ? 0x7FFFFFFF : -0x7FFFFFFF;
	return jo_fixed_div(s, c);
}

/*
 * Turning, scaling and the field of view by a jo_angle, as slRotX/slRotZ/slScale/slPerspective
 * did. jo's public ones (jo_3d_rotate_matrix_x, jo_3d_perspective_angle) take whole degrees,
 * so these call the functions underneath them. jo marks those "internal usage": still public
 * in v2026.2, but if a jo update renames them, this file is the one place to change
 */
static __jo_force_inline void angle_rotate_x(jo_angle angle)
{
	__jo_3d_rotate(JO_3D_AXIS_X, angle);
}

static __jo_force_inline void angle_rotate_z(jo_angle angle)
{
	__jo_3d_rotate(JO_3D_AXIS_Z, angle);
}

// Scale the current matrix (JO_FIXED_1: no change)
static __jo_force_inline void matrix_scale(jo_fixed x, jo_fixed y, jo_fixed z)
{
	__jo_3d_scale(x, y, z);
}

// The horizontal field of view: the whole angle across the screen
static __jo_force_inline void angle_perspective(jo_angle fov)
{
	__jo_3d_set_perspective(fov);
}
