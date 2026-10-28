#ifndef __DEPTH_FOG_H__
#define __DEPTH_FOG_H__

#include "world.h"
#include "floor_fade.h"

/*
 * Fog for walls and trails: how dark each corner should be, by its distance. Pure C,
 * unit-tested on the PC; object_fog_vdp1.c turns the levels into VDP1 Gouraud shading.
 *
 * The floor's fog (floor_fade.h) gives each screen row one strength, because every pixel of a
 * flat floor on one row is the same distance away. That doesn't hold for walls and trails: one
 * row can cross a near trail and a far wall. So here every corner gets its own strength, and
 * VDP1 blends between the four corners of each quad.
 *
 * "Distance" is the floor's: how far ahead of the camera along the floor, measured from the
 * point under the camera in the direction it faces, ignoring height. Then a wall stands on
 * floor of exactly its own fog strength, so the two fade together. It's a dot product with the
 * camera's forward direction (x1024), so it costs two multiplies and no division:
 *     depth x1024 = (x - eye_x) * forward_x + (y - eye_y) * forward_y
 *
 * Levels are the floor's 16 (0 = clear .. 15 = full fog). Which level a depth falls in is
 * found by comparing it with 15 thresholds worked out when the view changes, so the per-corner
 * cost is 4 comparisons (a binary search), not the division the floor does once per row.
 */

#define DEPTH_FOG_LEVELS FLOOR_FADE_LEVELS

typedef struct
{
	world_point eye; // the point on the floor under the camera
	int forward_x;	 // the camera's forward direction on the floor, x1024
	int forward_y;
	// thresholds[k]: the depth (x1024) where level k + 1 begins. Rising
	int thresholds[DEPTH_FOG_LEVELS - 1];
} depth_fog;

// Where the fade begins and where it's full, in world units along the floor: use the floor's
// own figures so the two match. Divides, so only when the view changes.
void depth_fog_set_distances(depth_fog *f, int fade_start, int fade_end);

// Where the camera is and which way it faces (forward x1024). Every frame.
void depth_fog_set_camera(depth_fog *f, world_point eye, int forward_x, int forward_y);

// The fog level (0..DEPTH_FOG_LEVELS - 1) at a point on the floor, or above it (height
// doesn't count). Per corner: no division.
int depth_fog_level(const depth_fog *f, int x, int y);

/*
 * VDP1's Gouraud shading colour for a level. Each Gouraud colour channel is 0..31, and VDP1
 * adds (channel - 16) to the polygon's own colour: 16 leaves it alone, 0 takes 16 off. So a
 * level can darken by at most 16 of the 31 steps of brightness: a bright grid line (~28) dims
 * to ~12, while dark navy goes black. Full fog dims; it can't reach black on its own.
 * Bit 15 is set, as VDP1 expects in RGB colours. Divides: for building a table at start-up.
 */
#define DEPTH_FOG_GOURAUD_NEUTRAL 0xC210 // all three channels 16: no change (jo's JO_NO_GOURAUD_COLOR)
unsigned short depth_fog_gouraud_colour(int level);

#endif
