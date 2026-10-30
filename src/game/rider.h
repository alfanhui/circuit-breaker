#ifndef __RIDER_H__
#define __RIDER_H__

#include "cycle.h"
#include "trail.h"
#include "turn_gate.h"

/*
 * Someone riding a cycle: the cycle, the trail it lays, and its turn gate. Pure C, unit-tested.
 *
 * Players and bots are both riders, and both are driven only through rider_controls: this
 * frame's turn press and whether boost is held. That's what makes a bot fair by construction:
 * it can't move, turn or boost in any way a player can't, because it goes through exactly the
 * same code (the gate's minimum gap between turns, the boost gauge, collision).
 */

typedef struct
{
	signed char turn;	 // this frame's turn press: TURN_LEFT, TURN_RIGHT or TURN_NONE (turn_gate.h)
	unsigned char boost; // 1 while boost is held
} rider_controls;

typedef struct
{
	cycle bike;
	trail wall;		 // the trail it lays, from its start to the cycle
	turn_gate turns; // a minimum gap between turns
} rider;

// A rider standing at (x, y), facing heading, with a fresh trail
void rider_init(rider *r, short x, short y, unsigned char heading);

// One game frame: turn (if the gate allows), move (stopping short of the arena edge and the
// given trails, collision.h), and grow the trail to the cycle. Returns the turn it made
// (TURN_LEFT, TURN_RIGHT or TURN_NONE), so the 1st person camera can ease round with it.
int rider_step(rider *r, rider_controls controls, const trail *const *trails, int trail_count);

#endif
