#ifndef __ATTRACT_ROUTE_H__
#define __ATTRACT_ROUTE_H__

#include "world.h"
#include "cycle.h"
#include "attract_text.h"

/*
 * Rides the prologue's loop: the cycle from one waypoint to the next (attract_text.h builds
 * them), round and round, before anyone takes over (docs/STORY.md). Pure C, unit-tested on the PC.
 *
 * It's driven like everything else, through a turn press (rider_controls, rider.h): the route
 * only says when to press and which way. "Reached" means the cycle has got to the waypoint or
 * gone past it, along the way it's heading (it moves several units a frame, so it rarely lands
 * on one exactly). It's then put on the waypoint exactly, so the letters' corners are square and
 * every loop is the same: a jump of a few units, too small to see from the stands.
 *
 * Headings: north is -y, east is -x (world.h). Turning right goes N, E, S, W.
 */

typedef struct
{
	const attract_waypoint *waypoints; // [0] is the start; the last is the start a period on
	unsigned short count;
	unsigned short next; // the waypoint it's riding towards
} attract_route;

// The route, riding from waypoints[0] towards waypoints[1]
void attract_route_init(attract_route *r, const attract_waypoint *waypoints, int count);

// Where it starts, and the way it faces there
world_point attract_route_start(const attract_route *r);
int attract_route_start_heading(const attract_route *r);

// Each frame, before the cycle moves. If it has reached the waypoint it's riding to, it's put on
// it exactly, and the route moves on to the next: returns that waypoint (so the caller can see
// whether the pen goes up or down there, or a word ends), else 0. *turn is the turn to press now
// (TURN_LEFT, TURN_RIGHT or TURN_NONE, turn_gate.h): none where the way on is straight ahead.
// After the last waypoint it reaches nothing more, and attract_route_done says so
const attract_waypoint *attract_route_step(attract_route *r, cycle *c, int *turn);

// Whether the trail is laid on the leg it's riding now
int attract_route_pen(const attract_route *r);

// 1 once it has reached the last waypoint: put the cycle back at the start, and init the route
int attract_route_done(const attract_route *r);

// The heading that rides from a to b (they must share an x or a y, and differ)
int attract_route_heading(world_point a, world_point b);

#endif
