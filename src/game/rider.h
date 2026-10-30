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
	unsigned char crashed; // 1 if it crashed this frame (cycle_update)
	unsigned char alive;   // 0 once killed: it no longer moves, and its trail sinks away
	unsigned char trail_height; // how tall its trail stands: TRAIL_HEIGHT, down to 0 as it sinks
} rider;

// How fast a dead rider's trail sinks into the floor: TRAIL_HEIGHT (60) in 1 s
#define RIDER_SINK_SPEED 1

// A rider standing at (x, y), facing heading, with a fresh trail
void rider_init(rider *r, short x, short y, unsigned char heading);

// A rider's process is terminated (a bot that crashed): it stops, and from now on rider_sink
// lowers its trail into the floor. Its trail should stop blocking anyone straight away
void rider_kill(rider *r);

// Each frame for a dead rider: its trail sinks RIDER_SINK_SPEED lower, and once it's into the
// floor it's removed. Returns 1 while there's still some of it to draw
int rider_sink(rider *r);

// One game frame: turn (if the gate allows), move (stopping short of the arena edge and the
// given trails, collision.h), and grow the trail to the cycle. Returns the turn it made
// (TURN_LEFT, TURN_RIGHT or TURN_NONE), so the 1st person camera can ease round with it.
int rider_step(rider *r, rider_controls controls, const trail *const *trails, int trail_count);

#endif
