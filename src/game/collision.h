#ifndef __COLLISION_H__
#define __COLLISION_H__

#include "world.h"
#include "trail.h"

/*
 * What stops a cycle: the arena edge and light cycle trails. Pure C, unit-tested on the PC.
 *
 * Asked before the cycle moves: "it wants to go `distance` this frame; how far can it go?"
 * The answer is distance if nothing is in the way, or less if it would run into something.
 * Checking before moving means the cycle never ends a frame inside a trail, so nothing is
 * ever drawn overlapping (no need to push it back out afterwards).
 *
 * Everything is axis-aligned (cycles only move N/E/S/W and trails are made of their paths),
 * so it's all comparisons and subtractions: no division, no multiplication by anything but
 * the heading's -1 / 0 / +1.
 *
 * The cycle is a point (its position: the trail is laid from it and the 1st person camera sits
 * on it). Instead of giving the cycle a size, each trail segment is made fatter by a clearance
 * on every side: the same test, simpler sums.
 *
 * A cycle is often inside a fattened segment of its own trail: always its live segment (the
 * trail grows from the cycle), and for the first few units after a turn, the segment it just
 * finished. It's driving out of those, so they don't stop it. What does: any segment it would
 * cross into, and any it's inside with part of the segment still ahead of it. That's a cycle
 * turned round on the spot (stopped cycles can always turn) facing back down the trail it just
 * laid. Moving N/E/S/W, there's no other way to be inside a segment.
 */

// Half a cycle's width (it's 50 wide, with the trail down its centre). A cycle stops 25 short of
// a trail, the closest it can be without overlapping. The trail is cut TRAIL_MIN_FORWARD (10) in
// front of the 1st person camera, and that cut must stay off-screen even 25 to the side, turning
// along the trail after a crash: at the old 20 it showed as a hole (trail_pieces.h).
// The turn gate's 50-unit U-turn (turn_gate.h) leaves exactly this much spare beside the trail.
#define COLLISION_TRAIL_CLEARANCE 25

// How far a cycle at from, heading this way, can go (up to distance) before it comes within
// clearance of the segment a-b. Returns distance when the segment isn't in the way.
int collision_segment(world_point from, int heading, int distance, world_point a, world_point b, int clearance);

// The same, for every segment of a trail (with COLLISION_TRAIL_CLEARANCE).
int collision_trail(world_point from, int heading, int distance, const trail *t);

// How far it can go before a coordinate passes -limit..limit (the arena edge). 0 if it's already
// at or past the edge in the direction it's heading.
int collision_edge(world_point from, int heading, int distance, int limit);

// Everything together: the edge (limit), then each of trail_count trails (trails may be NULL
// when there are none). A result less than distance is a crash.
int collision_free_distance(world_point from, int heading, int distance, int limit,
							const trail *const *trails, int trail_count);

#endif
