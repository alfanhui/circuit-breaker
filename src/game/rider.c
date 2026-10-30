#include "rider.h"

void rider_init(rider *r, short x, short y, unsigned char heading)
{
	cycle_init(&r->bike, x, y, heading);
	turn_gate_init(&r->turns);
	trail_reset(&r->wall, r->bike.position);
	r->crashed = 0;
	r->alive = 1;
	r->trail_height = TRAIL_HEIGHT;
}

void rider_kill(rider *r)
{
	r->alive = 0;
}

int rider_sink(rider *r)
{
	if (r->alive || r->trail_height == 0)
		return r->trail_height > 0;
	r->trail_height = (unsigned char)(r->trail_height > RIDER_SINK_SPEED ? r->trail_height - RIDER_SINK_SPEED : 0);
	if (r->trail_height == 0)
		trail_reset(&r->wall, r->bike.position); // into the floor: removed (a lone point, no segments)
	return r->trail_height > 0;
}

int rider_step(rider *r, rider_controls controls, const trail *const *trails, int trail_count)
{
	world_point before;
	int dx;
	int dy;
	// The gate holds back a turn pressed within a cycle-width of the last one (a quick double
	// tap) until that gap is reached, so a U-turn never lands on the cycle's own trail
	int turn = turn_gate_update(&r->turns, controls.turn);

	// Turns are instant: the corner is pinned where the cycle stands, then it faces the new way
	if (turn == TURN_LEFT)
	{
		trail_add_corner(&r->wall);
		cycle_turn_left(&r->bike);
	}
	else if (turn == TURN_RIGHT)
	{
		trail_add_corner(&r->wall);
		cycle_turn_right(&r->bike);
	}

	before = r->bike.position;
	r->crashed = (unsigned char)cycle_update(&r->bike, controls.boost, trails, trail_count);
	// How far it went: it only moves along one axis, so the two differences add up to it
	dx = r->bike.position.x - before.x;
	dy = r->bike.position.y - before.y;
	turn_gate_moved(&r->turns, (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
	trail_set_head(&r->wall, r->bike.position);
	return turn;
}
