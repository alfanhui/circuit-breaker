#ifndef __VIEW_CULL_H__
#define __VIEW_CULL_H__

#include "world.h"

/*
 * The part of the floor each camera can see, as a rectangle, so walls and trails outside it
 * are never sent to SGL. Pure C, unit-tested on the PC.
 *
 * Why not leave it to SGL: SGL transforms every vertex it's given (the main CPU cost), and in
 * testing passed ~190 of 274 trail pieces to VDP1 in the iso view when ~20 were on screen.
 * It mostly drops only what's behind the camera; VDP1 then spends time on the rest.
 *
 * A rectangle is a loose fit (the real view is a wedge), but it's cheap to test against and
 * the walls and trails are axis-aligned strips, so clipping them to it is simple.
 */

typedef enum
{
	VIEW_CULL_FIRST_PERSON, // looking along the cycle's heading
	VIEW_CULL_TURNING,		// 1st person camera easing between two headings: could face either
	VIEW_CULL_ISO,			// fixed high camera looking down at the cycle
	VIEW_CULL_ALL			// debug camera: anywhere, so everything in range
} view_cull_mode;

// 1st person: 90 degree FOV, so the view widens one unit sideways per unit forward
#define VIEW_CULL_BEHIND 300 // pieces straddling the camera; SGL drops what's really behind
// Iso: the camera sees ~1100 from the cycle; walls nearer the camera than the cycle are taller
// than it (1536 vs 768) and can rise into the bottom of the screen, hence the margin
#define VIEW_CULL_ISO_RANGE 2000

// range: how far ahead (and to the sides) to look: WORLD_VIEW_RANGE for trails,
// WORLD_WALL_VIEW_RANGE for walls. The iso view ignores it (VIEW_CULL_ISO_RANGE).
world_rect view_cull_rect(world_point eye, int heading, view_cull_mode mode, int range);

#endif
