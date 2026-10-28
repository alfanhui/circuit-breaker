#ifndef __MINIMAP_H__
#define __MINIMAP_H__

#include "world.h"
#include "trail.h"

/*
 * The 1st person radar: every trail near the player, seen from above, turned so the way the
 * camera faces is always up (like a car's sat-nav, not a fixed north-up map). Pure C,
 * unit-tested on the PC; hud.c draws the lines it works out.
 *
 * It shows a circle of MINIMAP_RANGE world units round the player in MINIMAP_RADIUS pixels.
 * 2048 units to 32 pixels is / 64: a shift (MINIMAP_SHIFT), so no division.
 *
 * Each frame, for each trail segment:
 *   1. clip it to the circle, in world space. Segments only run N/S/E/W, so a segment a
 *      distance c from the player (across its own direction) is inside the circle for
 *      +- sqrt(RANGE^2 - c^2) along it. Those half-widths come from a table worked out once at
 *      start-up (minimap_init): no square root per frame.
 *   2. turn its two ends round the player so the camera's forward is up, and scale to pixels.
 *      The camera's forward is a unit vector x1024 (as in wall_clip.h), so turning a point is
 *      two multiplies per axis, and one shift does both the x1024 and the / 64.
 * The circle doesn't change as it turns, so clipping first and turning after is exact.
 */

#define MINIMAP_RANGE 2048 // world units from the player to the map's edge
#define MINIMAP_SHIFT 6	   // world units to pixels: >> 6 is / 64
#define MINIMAP_RADIUS (MINIMAP_RANGE >> MINIMAP_SHIFT) // 32 pixels
#define MINIMAP_LINES_MAX 128

// Half-widths are kept for bands of this many world units across (64 bands over the range)
#define MINIMAP_BAND_SHIFT 5

typedef struct
{
	signed char x0, y0; // pixels from the map's centre, +x right, +y down (forward is up)
	signed char x1, y1;
	unsigned char trail; // which of the trails it came from: its colour
} minimap_line;

// Works out the circle's half-widths. Once, at start-up (it's the only place with a square root).
void minimap_init(void);

// How far along a N/S/E/W line the circle reaches, `across` world units from the player.
// 0 when the line misses the circle.
int minimap_half_width(int across);

// The lines to draw for every trail, round `eye`, with the camera facing (forward_x, forward_y)
// (a unit vector x1024). Returns how many were written (at most max_lines).
int minimap_build(const trail *const *trails, int trail_count, world_point eye, int forward_x, int forward_y,
				  minimap_line *out, int max_lines);

#endif
