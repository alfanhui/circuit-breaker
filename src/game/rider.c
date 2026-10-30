#include "rider.h"

void rider_init(rider *r, short x, short y, unsigned char heading)
{
	cycle_init(&r->bike, x, y, heading);
	turn_gate_init(&r->turns);
	trail_reset(&r->wall, r->bike.position);
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
	cycle_update(&r->bike, controls.boost, trails, trail_count);
	// How far it went: it only moves along one axis, so the two differences add up to it
	dx = r->bike.position.x - before.x;
	dy = r->bike.position.y - before.y;
	turn_gate_moved(&r->turns, (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
	trail_set_head(&r->wall, r->bike.position);
	return turn;
}
