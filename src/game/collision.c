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

/*
 * collision_trail is asked about every segment of every trail several times a frame (each cycle
 * moving, and each bot looking around), so it's the hot path. Calling collision_segment per
 * segment, one full trail (stress_cpu.steps) took 53% of a frame in all, and this way 38%; with
 * two busy trails frames dropped below 60 fps (busy_play.steps). It gives exactly the same answers
 * as calling collision_segment on each segment (unit-tested against it), faster:
 *   - the heading is turned into "which axis, which way" once per question, not 6 times per
 *     segment (WORLD_HEADING_DX / DY are chains of comparisons)
 *   - it walks the corners directly rather than fetching each segment through a function
 *   - a segment that isn't across the cycle's path is thrown away after 2 comparisons, before
 *     any of the along sums. Most segments are: a cycle's path is a line 50 wide
 */

// One end of a segment, as across and along for this question
#define ACROSS(p) (along_x ? (p).y : (p).x)
#define ALONG(p) (along_x ? sign * (p).x : sign * (p).y)

int collision_trail(world_point from, int heading, int distance, const trail *t)
{
	const int along_x = WORLD_HEADING_DX(heading) != 0; // moving along x (east or west)
	const int sign = along_x ? WORLD_HEADING_DX(heading) : WORLD_HEADING_DY(heading);
	const int from_across = ACROSS(from);
	const int from_along = ALONG(from);
	const int corners = t->corner_count;

	// Every segment, oldest to newest, including the live one the cycle is laying (it's always
	// inside that one, so it never stops it). Each hit can only shorten the distance, so the
	// nearest obstacle wins whatever order they're checked in
	for (int i = 0; i < corners; ++i)
	{
		world_point a = t->corners[i];
		// The last corner joins the head: the live segment (none if the head is still on it)
		world_point b = i + 1 < corners ? t->corners[i + 1] : t->head;
		int a_across = ACROSS(a);
		int b_across = ACROSS(b);
		int a_along;
		int b_along;
		int near_edge;

		if (a.x == b.x && a.y == b.y)
			continue;
		// Beside the path, not across it: never in the way (as collision_segment, strictly inside)
		if (from_across <= (a_across < b_across ? a_across : b_across) - COLLISION_TRAIL_CLEARANCE ||
			from_across >= (a_across > b_across ? a_across : b_across) + COLLISION_TRAIL_CLEARANCE)
			continue;

		a_along = ALONG(a);
		b_along = ALONG(b);
		near_edge = (a_along < b_along ? a_along : b_along) - COLLISION_TRAIL_CLEARANCE;
		if (near_edge < from_along)
		{
			// Inside it or past it: only a crash if part of it is still ahead (collision_segment)
			if ((a_along > b_along ? a_along : b_along) > from_along)
				return 0;
		}
		else if (near_edge - from_along < distance)
			distance = near_edge - from_along;
	}
	return distance;
}

#undef ACROSS
#undef ALONG

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
