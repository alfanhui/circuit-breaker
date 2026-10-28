#include "collision.h"

/*
 * Every check works in two coordinates relative to the heading, so one piece of code covers all
 * four directions:
 *   along:  how far forward a point is, growing in the direction of travel
 *           (heading east is -x in this world, so along = -x)
 *   across: the other axis, the one the cycle isn't moving on
 */
static int along(world_point p, int heading)
{
	return WORLD_HEADING_DX(heading) * p.x + WORLD_HEADING_DY(heading) * p.y;
}

static int across(world_point p, int heading)
{
	return WORLD_HEADING_DX(heading) != 0 ? p.y : p.x;
}

int collision_segment(world_point from, int heading, int distance, world_point a, world_point b, int clearance)
{
	int a_along = along(a, heading);
	int b_along = along(b, heading);
	int a_across = across(a, heading);
	int b_across = across(b, heading);
	int from_across = across(from, heading);
	int from_along = along(from, heading);
	int far_end = a_along > b_along ? a_along : b_along; // the segment's furthest point ahead
	// The fattened segment's near edge: the side the cycle would cross first
	int near_edge = (a_along < b_along ? a_along : b_along) - clearance;
	int gap = near_edge - from_along;

	// Beside it rather than in line with it: it's never in the way. Strictly inside, so a cycle
	// exactly clearance away (a 50-wide U-turn back beside its own trail) slides past
	if (from_across <= (a_across < b_across ? a_across : b_across) - clearance ||
		from_across >= (a_across > b_across ? a_across : b_across) + clearance)
		return distance;
	if (gap < 0)
	{
		// The near edge is behind it: it's past the segment, or inside it. Inside is normal for
		// its own live segment (the trail grows from the cycle, all of it behind) and, after a
		// turn, the one it just finished (side-on, through the corner it's standing on): either
		// way it's driving out. But turned round on the spot (a stopped cycle can always turn),
		// the trail it just laid is ahead of it, along its path: driving on would go back over
		// it. Any of the segment still ahead of the cycle means that: a crash, it can't move
		return far_end > from_along ? 0 : distance;
	}
	// gap >= distance: out of reach this frame (at exactly distance it only touches the edge)
	return gap < distance ? gap : distance;
}

int collision_trail(world_point from, int heading, int distance, const trail *t)
{
	world_point a;
	world_point b;
	int count = trail_segment_count(t);

	// Every segment, oldest to newest, including the live one the cycle is laying (it's always
	// inside that one, so it never stops it). Each hit can only shorten the distance, so the
	// nearest obstacle wins whatever order they're checked in. Up to 64 segments of a few
	// comparisons each: cheap enough every frame
	for (int i = 0; i < count; ++i)
	{
		if (trail_get_segment(t, i, 0, &a, &b))
			distance = collision_segment(from, heading, distance, a, b, COLLISION_TRAIL_CLEARANCE);
	}
	return distance;
}

int collision_edge(world_point from, int heading, int distance, int limit)
{
	// The edge ahead is at along = limit whichever way it's heading (along flips the sign for us)
	int gap = limit - along(from, heading);

	if (gap < 0)
		return 0; // already past it (the arena shrank, or the boundary came back on): go no further
	return gap < distance ? gap : distance;
}

int collision_free_distance(world_point from, int heading, int distance, int limit,
							const trail *const *trails, int trail_count)
{
	distance = collision_edge(from, heading, distance, limit);
	for (int i = 0; i < trail_count; ++i)
		distance = collision_trail(from, heading, distance, trails[i]);
	return distance;
}
