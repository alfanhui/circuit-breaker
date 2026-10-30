#ifndef __WALL_CLIP_H__
#define __WALL_CLIP_H__

#include "world.h"

/*
 * Keeps a strip of wall pieces drawable for VDP1, in the 1st person view. Pure C, unit-tested.
 *
 * Two hardware limits break walls close to the camera:
 *   - SGL projects every corner it's given, even one behind the camera; that corner lands on
 *     the wrong side of the screen and the quad's texture bends.
 *   - VDP1 corner coordinates must stay within -1024..1023 pixels (11 bits; beyond that they
 *     wrap). A corner 1465 above the eye projects to 160 * 1465 / distance pixels, so it must be
 *     at least ~230 units in front of the camera.
 * So each strip has a minimum distance in front of the camera (its height decides it). The
 * strip's near end slides along the wall until it's that far in front. VDP1 stretches the whole
 * texture over each quad, so sliding a corner just squeezes the texture sideways; the wall
 * texture has lines only at piece edges, so nothing visible moves.
 * Pieces wholly outside the view (90 degrees, wider while boosting) are dropped, so far-off
 * pieces can't overflow either.
 *
 * A strip is columns[0..n] (n pieces) along a straight wall.
 */

#define WALL_CLIP_FOV_MARGIN 100 // keep pieces just outside the view edge, so none pop at the border

// How far the view reaches sideways per unit in front, x16: tan(half the FOV) x 16. A 90
// degree view reaches as far to the side as it is in front: 16. The boost widens the view (view.c)
#define WALL_CLIP_VIEW_WIDTH_90 16

typedef struct
{
	world_point eye;
	int forward_x;	// camera's forward direction on the floor, x1024
	int forward_y;
	int view_width; // sideways reach per unit in front, x16 (WALL_CLIP_VIEW_WIDTH_90 = 90 degrees)
} wall_clip_camera;

typedef struct
{
	int first;		   // first visible piece
	int count;		   // visible pieces (0: none)
	world_point start; // column `first`, possibly slid forward
	world_point end;   // column `first + count`, possibly slid forward
} wall_clip_run;

// Visible run of pieces for a strip whose corners must be at least min_forward in front.
wall_clip_run wall_clip_strip(const world_point *columns, int pieces,
							  const wall_clip_camera *camera, int min_forward);

// Whether to draw an object (another cycle) at p, reaching radius from its centre: all of it at
// least min_forward in front of the camera, and not wholly off either side of the view. An
// object even partly behind the camera is skipped: SGL would project its corners behind the
// camera onto the screen (a cycle behind the player showed up in front of them)
int wall_clip_object_visible(const wall_clip_camera *camera, world_point p, int radius, int min_forward);

#endif
