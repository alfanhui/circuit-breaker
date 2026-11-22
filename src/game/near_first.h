#ifndef __NEAR_FIRST_H__
#define __NEAR_FIRST_H__

/*
 * An order to visit things in, nearest first (the user, 2026-11-16: from the CD block's lift "the
 * city below when looking down is cut nearest to the player").
 *
 * The city's towers are drawn into one mesh with room for so many faces (level.c, GREY_POINTS_MAX),
 * and a face that doesn't fit is left out for that frame. They went in block by block in the plan's
 * order, row 0 first. Up the lift, with no roof under you to hide anything (city_seen), the whole
 * city is in view, the mesh filled up part way, and what was left out was the end of the list: the
 * hub's own rows, right under you. Visited nearest first, what's left out is the furthest: small,
 * and deep in the fog.
 *
 * It only needs to be roughly right: a near block a little after a nearer one makes no difference
 * unless the mesh fills just there. So each thing has a bucket, its distance >> NEAR_FIRST_SHIFT,
 * and the order is a counting sort by bucket: two passes, no comparisons, no division, and things
 * in one bucket keep their own order (stable). Pure C: unit-tested on the PC (tests/test_near_first.c)
 */

#define NEAR_FIRST_SHIFT 9	 // a bucket is 512 units: about a tower and its street
#define NEAR_FIRST_BUCKETS 32 // further than 31 buckets (~16000) all go last, together
#define NEAR_FIRST_MAX 80	 // things it can order (the city's blocks: 64, level.c GREY_GROUPS)

// order[0 .. count-1]: the indices 0 .. count-1, nearest first by distance[i] (any measure that
// grows with distance: level.c gives |dx| + |dy|). Negative distances count as 0
void near_first_order(const int *distance, int count, unsigned char *order);

#endif
