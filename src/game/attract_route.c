#include "attract_route.h"
#include "turn_gate.h"

void attract_route_init(attract_route *r, const attract_waypoint *waypoints, int count)
{
	r->waypoints = waypoints;
	r->count = (unsigned short)count;
	r->next = 1;
}

int attract_route_heading(world_point a, world_point b)
{
	if (b.x == a.x)
		return b.y < a.y ? HEADING_NORTH : HEADING_SOUTH; // north is -y
	return b.x < a.x ? HEADING_EAST : HEADING_WEST;		  // east is -x
}

world_point attract_route_start(const attract_route *r)
{
	return r->waypoints[0].p;
}

int attract_route_start_heading(const attract_route *r)
{
	return attract_route_heading(r->waypoints[0].p, r->waypoints[1].p);
}

// Has the cycle got to the point, or past it, along the way it's heading?
static int reached(const cycle *c, world_point p)
{
	switch (c->heading)
	{
	case HEADING_NORTH:
		return c->position.y <= p.y;
	case HEADING_SOUTH:
		return c->position.y >= p.y;
	case HEADING_EAST:
		return c->position.x <= p.x;
	default: // west
		return c->position.x >= p.x;
	}
}

const attract_waypoint *attract_route_step(attract_route *r, cycle *c, int *turn)
{
	const attract_waypoint *here;
	int after;

	*turn = TURN_NONE;
	if (r->next >= r->count || !reached(c, r->waypoints[r->next].p))
		return 0; // done, or still riding to it
	here = &r->waypoints[r->next];
	c->position = here->p; // exactly on it: square corners, the same every loop
	r->next++;
	if (r->next >= r->count)
		return here; // the last: the loop's done

	// The way the next leg goes. Turning right goes N, E, S, W: one step on (& 3 wraps W round to
	// N); the same way is straight on; anything else is left (attract_text never turns straight back)
	after = attract_route_heading(here->p, r->waypoints[r->next].p);
	if (after == ((c->heading + 1) & 3))
		*turn = TURN_RIGHT;
	else if (after != c->heading)
		*turn = TURN_LEFT;
	return here;
}

int attract_route_pen(const attract_route *r)
{
	return r->next < r->count && r->waypoints[r->next].pen;
}

int attract_route_done(const attract_route *r)
{
	return r->next >= r->count;
}
