#ifndef __OCCLUDER_H__
#define __OCCLUDER_H__

#include <stdbool.h>
#include "frustum.h"

/*
 * What a wall hides, and what's seen through a hole (docs/BACKLOG.md: "Looking back down the
 * corridor from the city"). VDP1 has no depth buffer: everything sent is filled, even what's behind
 * something nearer. Looking down the corridor from the city, the arena's walls were all drawn
 * behind the corridor's, unseen: 65 polygons, ~22% of the frame, for a picture identical without them.
 *
 * One shape does both jobs: a rectangle standing upright on a line along x or y (a wall, or a
 * hole in one). From the eye, its four edges make four planes (frustum_add_edge), a pyramid with
 * the eye at its point, out through the rectangle and on for ever. Add a fifth, the rectangle's own
 * plane, and the part of the pyramid past it is its shadow:
 *   - as a wall, occluder_hides: a box wholly inside the shadow can't be seen. Every ray from the
 *     eye to any part of it passes through the wall first
 *   - as a hole, occluder_shows: a box wholly outside the pyramid's sides can't be seen through
 *     it. Behind the hole's wall, that's all of it hidden
 * A few multiplies a plane, stopping at the first that settles it: no division per box. The planes are
 * worked out once a frame for each rectangle (frustum_add_edge: a few divisions each).
 *
 * Its corners must be within 16384 of the eye (frustum_add_edge's sums); a box tested, within 32000.
 * Pure C, no SGL: unit-tested on the PC (tests/test_occluder.c); level.c uses it on foot
 */

// Nearer its line than this, a rectangle hides nothing and shows everything: the planes from an eye
// in the hole are the hole itself, and a wall that near may be in front of the near plane, so not
// drawn, and not hiding anything
#define OCCLUDER_NEAREST 64

typedef struct
{
	frustum f;		// the eye and the five planes
	unsigned sides; // the pyramid's four sides' bits (planes 0 .. 3)
	int own;		// the rectangle's own plane (4): past it and inside the sides, its shadow
	bool on;		// false: too near, or the eye in its plane (hides nothing, shows everything)
} occluder;

/*
 * The rectangle on the line along_x ? y = line : x = line, from `from` to `to` along it (either way
 * round), and from z bottom to top, as seen from eye
 */
void occluder_set(occluder *o, frustum_point eye, bool along_x, int line, int from, int to, int bottom, int top);

// Whether the box (centre, half its size each way, as frustum_classify_box) is wholly behind it
bool occluder_hides(const occluder *o, frustum_point centre, frustum_point half);

// Whether any of the box could be seen through it, as a hole (it's not wholly outside the pyramid)
bool occluder_shows(const occluder *o, frustum_point centre, frustum_point half);

#endif
