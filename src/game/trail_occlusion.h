#ifndef __TRAIL_OCCLUSION_H__
#define __TRAIL_OCCLUSION_H__

#include "trail.h"

/*
 * Which parts of a trail are completely hidden behind a nearer trail, in the 1st person view,
 * so they're not drawn at all. Pure C, unit-tested on the PC.
 *
 * Why: VDP1 has no depth buffer. SGL draws polygons back to front, judging each by its centre's
 * depth: its distance straight ahead of the camera. Riding alongside two parallel trails, a
 * piece of each, side by side, are at almost the same depth ahead (how far to the side doesn't
 * count), so which is drawn last, on top, flips from frame to frame: the farther trail showed
 * through the nearer one. Not drawing what's hidden fixes that, and saves drawing it.
 *
 * When one trail hides another: the eye is 35 above the floor and trails are 60 tall, so the
 * eye is below their tops. Then a nearer trail running parallel to a farther one, on the same
 * side of the eye, hides the farther one completely, top to bottom, wherever it's in the way.
 *
 * Where it's in the way: picture the trails from above. Measure across (sideways from the eye)
 * and along (the way the trails run, from the eye). The near trail is d1 across, the far one d2.
 * A line from the eye to a point t along the far trail crosses the near trail's line at
 * t * d1 / d2 along. So the far trail is hidden for t from A * d2 / d1 to B * d2 / d1, where
 * the near trail runs from A to B along. That's two divisions per pair of trails that line up
 * like this (a few a frame): not per piece.
 *
 * Only segments running the same way are compared (every trail runs N/S/E/W). Diagonal ones
 * (the debug stress trail's head) are never hidden and hide nothing.
 */

#define TRAIL_OCCLUDERS_MAX 32 // nearby segments that can hide others: the nearest ones in view
#define TRAIL_VISIBLE_PARTS_MAX 8 // a segment can be left in at most this many visible parts

// Each listed segment as seen from the eye, worked out once when it's added: it's compared with
// every visible segment each frame (~100 x 32 pairs with busy trails), so the per-pair work must
// be a few comparisons. (Working it out again per pair was part of a slowdown to 48 fps)
typedef struct
{
	world_point eye;
	int count;
	unsigned char along_x[TRAIL_OCCLUDERS_MAX]; // 1: runs east-west (along x, across y); 0: north-south
	int across[TRAIL_OCCLUDERS_MAX];			// distance across from the eye (signed: which side)
	int along_min[TRAIL_OCCLUDERS_MAX];			// where it starts and ends along, from the eye,
	int along_max[TRAIL_OCCLUDERS_MAX];			// capped to +-8192 (nothing further is drawn)
} trail_occluders;

// Starts a frame's list, seen from eye (the 1st person camera, on the floor under it)
void trail_occluders_begin(trail_occluders *o, world_point eye);

// Adds a trail's segments inside view (drawn with head_gap, as trail_pieces.h) to the list,
// up to TRAIL_OCCLUDERS_MAX
void trail_occluders_add(trail_occluders *o, const trail *t, int head_gap, const world_rect *view);

/*
 * The parts of the segment from -> to between steps s0 and s1 (steps along it, as in
 * trail_pieces.h) that aren't hidden behind a listed segment. Writes each part's first and last
 * step to starts / ends, in order, and returns how many (0: all hidden). If it would need more
 * than max parts, it stops cutting: it may draw a little more than it has to, never less.
 */
int trail_occluders_visible(const trail_occluders *o, world_point from, world_point to, int s0, int s1,
							int *starts, int *ends, int max);

#endif
