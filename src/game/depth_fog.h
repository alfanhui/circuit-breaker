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

/*
 * The same darkening done to a flat colour (an RGB555 jo_color, bit 15 set) instead: each 5-bit
 * channel `steps` darker (16 - a Gouraud channel, as depth_fog_gouraud_colour's), not below 0. For
 * a small untextured polygon one fog level for the whole of it is enough, and drawing it in the
 * darker colour costs VDP1 nothing: no Gouraud table to write, no shading worked out per pixel
 */
unsigned short depth_fog_darken(unsigned short colour, int steps);

/*
 * Gouraud tables shared by every quad that needs the same one, written once at start-up instead
 * of one per quad every frame. A wall or trail piece runs between two columns, and a column's top
 * and bottom are the same distance along the floor, so the same level: its four corners are only
 * ever two levels, a and b, in one of two orders round it:
 *   - a a b b: listed from column one's bottom, up, across, down (the trails, upright walls)
 *   - a b b a: listed from column one's top, across, down, back (the arena's strips, lying walls)
 * With 16 levels that's 256 tables each, 512 in all (4 KB of VDP1 RAM). Writing a table into VDP1
 * RAM stops VDP1 drawing while the CPU does it (SEGA's VDP1 manual: the CPU always wins), and
 * hundreds were written every frame while it drew. Now only a quad whose corners are neither
 * (a face cut by the view, on foot) gets one of its own.
 * Table t: a a b b is t = a x 16 + b; a b b a is 256 + a x 16 + b
 */
#define DEPTH_FOG_SHARED_TABLES (2 * DEPTH_FOG_LEVELS * DEPTH_FOG_LEVELS)

// The shared table with these corners' levels (in the quad's vertex order), or -1 if none has them
int depth_fog_shared_table(int level0, int level1, int level2, int level3);

// The levels at a shared table's four corners, to write it (at start-up)
void depth_fog_shared_levels(int table, int levels[4]);

#endif
