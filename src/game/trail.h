#ifndef __TRAIL_H__
#define __TRAIL_H__

#include "world.h"

/*
 * A light cycle wall: a polyline of corners plus a live head that follows the cycle.
 * Pure C, fixed memory, so it compiles on the host for unit tests.
 *
 * Segments run corners[0] -> corners[1] -> ... -> corners[count - 1] -> head.
 * The last (live) segment grows every frame until the next turn pins it as a corner.
 */

#define TRAIL_MAX_CORNERS 64
#define TRAIL_MAX_SEGMENTS TRAIL_MAX_CORNERS // count - 1 committed + 1 live

typedef struct
{
	world_point corners[TRAIL_MAX_CORNERS]; // corners[0] is the oldest end of the wall
	unsigned short corner_count;
	world_point head;
} trail;

// Starts a new wall at start, discarding the old one.
void trail_reset(trail *t, world_point start);

// Moves the live end of the wall, normally to the cycle position every frame.
void trail_set_head(trail *t, world_point head);

// Pins the head as a corner (call on every turn). When full, the oldest corner is dropped.
void trail_add_corner(trail *t);

// Committed segments plus the live segment when it has length.
int trail_segment_count(const trail *t);

// Gets segment index (0 = oldest). The live segment is shortened by head_gap so the wall
// can start behind the cycle. Returns 0 when the index is out of range or the segment is empty.
int trail_get_segment(const trail *t, int index, int head_gap, world_point *from, world_point *to);

#endif
