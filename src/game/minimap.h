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
 * 1024 units to 32 pixels is / 32: a shift (MINIMAP_SHIFT), so no division.
 *
 * Each frame, for each trail segment:
 *   1. clip it to the circle, in world space. Segments only run N/S/E/W, so a segment a
 *      distance c from the player (across its own direction) is inside the circle for
 *      +- sqrt(RANGE^2 - c^2) along it. Those half-widths come from a table worked out once at
 *      start-up (minimap_init): no square root per frame.
 *   2. turn its two ends round the player so the camera's forward is up, and scale to pixels.
 *      The camera's forward is a unit vector x1024 (as in wall_clip.h), so turning a point is
 *      two multiplies per axis, and one shift does both the x1024 and the / 32.
 * The circle doesn't change as it turns, so clipping first and turning after is exact.
 */

// The zoom. It's for close quarters (trails near the player matter most), so 1024 units round the
// player fill the map: 32 world units per pixel, a trail 50 away is 1.5 pixels off. 2048 (shift
// 6) showed more of the arena, but nearby trails crowded into the middle.
#define MINIMAP_RADIUS 32 // pixels from the map's centre to its edge
#define MINIMAP_SHIFT 5	  // world units to pixels: >> 5 is / 32
#define MINIMAP_RANGE (MINIMAP_RADIUS << MINIMAP_SHIFT) // 1024 world units from the player to the edge
#define MINIMAP_LINES_MAX 128

// Half-widths are kept for 64 bands across the range (2 * radius bands, so 1 << (shift - 1) units each)
#define MINIMAP_BAND_SHIFT (MINIMAP_SHIFT - 1)

typedef struct
{
	signed char x0, y0; // pixels from the map's centre, +x right, +y down (forward is up)
	signed char x1, y1;
	unsigned char trail; // which of the trails it came from: its colour
} minimap_line;

/*
 * The tilt: the map is drawn lying back, like a hologram disc floating in front of the player,
 * seen in perspective. Picture the map as a real disc, tipped back by MINIMAP_TILT degrees, seen
 * from MINIMAP_TILT_DISTANCE pixels away. A map point y pixels below the centre is then
 *   depth  = DISTANCE - y * sin(tilt)        (the near half is closer, the far half further)
 *   height = y * cos(tilt)                   (tipping back squashes it)
 * and perspective divides both across and down by the depth: screen x = x * k / depth,
 * screen y = height * k / depth. k is chosen so the near edge keeps its flat width.
 * That turns the circle into a true ellipse whose far half is smaller, and keeps straight lines
 * straight. (It was a linear stretch from top to bottom, like VDP1 stretching a texture: the
 * circle came out as a dome, "a bent bicycle tyre", and straight lines didn't stay straight.)
 * The depth only depends on the row, and map points are whole pixels, so there are only
 * 2 * MINIMAP_EDGE + 1 rows: each row's screen y and x scale are worked out once at start-up
 * (that's where the division is), and a point costs a lookup and a multiply.
 */
#define MINIMAP_EDGE (MINIMAP_RADIUS + 1) // the disc's edge, just outside the furthest line
#define MINIMAP_TILT_SIN 784			   // the tilt, 50 degrees: sin and cos x1024
#define MINIMAP_TILT_COS 658
#define MINIMAP_TILT_DISTANCE 120 // pixels from the eye: smaller is stronger perspective

/*
 * The boost gauge is the map's rim: a band between MINIMAP_GAUGE_INNER and MINIMAP_GAUGE_OUTER,
 * lying on the tilted map. It's lit from the bottom (nearest the player) up both sides at once,
 * meeting at the top when full; as it drains, both lit arcs shrink back down.
 * Each side is MINIMAP_GAUGE_STEPS segments of 15 degrees.
 */
#define MINIMAP_GAUGE_STEPS 12
#define MINIMAP_GAUGE_OUTER MINIMAP_EDGE
#define MINIMAP_GAUGE_INNER (MINIMAP_EDGE - 3)

// How much of each side is lit for a gauge level 0..max, in 16ths of a segment
// (0..MINIMAP_GAUGE_STEPS * 16). max must be a constant: the compiler then works out the
// reciprocal, so it's a multiply and a shift, no division at run time. Rounded up so a full
// gauge lights it all
#define MINIMAP_GAUGE_LIT(level, max) \
	(((level) * ((MINIMAP_GAUGE_STEPS * 16 * 65536 + (max) - 1) / (max))) >> 16)

// The k-th point (0 at the bottom .. MINIMAP_GAUGE_STEPS at the top) up the right-hand side of
// the rim, radius map pixels from the centre (flat, before the tilt). The left side is the mirror
// image: negate x.
void minimap_gauge_point(int k, int radius, int *x, int *y);

/*
 * Which way something is, as a point on the rim: for a marker showing where a rider is, however
 * far (a bot beyond the radar's range is otherwise impossible to find in an arena this size).
 * dx, dy: where it is from the player; forward: the camera's facing, x1024 (as minimap_build).
 * Writes the nearest of the rim's points, every 15 degrees (minimap_gauge_point's k, 0 behind
 * .. MINIMAP_GAUGE_STEPS ahead) and the side (0 right, 1 left). How: turn it into the camera's
 * frame (right, ahead), then score each 15 degree direction by how well it points that way (a dot
 * product with its sine and cosine) and keep the best. 13 tries, 2 multiplies each: no division
 * and no square root, which finding the exact angle would need
 */
void minimap_direction(int dx, int dy, int forward_x, int forward_y, int *side, int *k);

// Works out the circle's half-widths and the tilt's rows. Once, at start-up (the only place with
// a square root or a division).
void minimap_init(void);

// Where a map point (pixels from the centre, as in minimap_line) lands on the tilted map, in
// pixels from its centre. x is still in map pixels: the caller widens it for the TV.
void minimap_tilt(int x, int y, int *screen_x, int *screen_y);

// How far along a N/S/E/W line the circle reaches, `across` world units from the player.
// 0 when the line misses the circle.
int minimap_half_width(int across);

// The lines to draw for every trail, round `eye`, with the camera facing (forward_x, forward_y)
// (a unit vector x1024). Returns how many were written (at most max_lines).
int minimap_build(const trail *const *trails, int trail_count, world_point eye, int forward_x, int forward_y,
				  minimap_line *out, int max_lines);

#endif
