#ifndef __TURN_GATE_H__
#define __TURN_GATE_H__

/*
 * A minimum gap between turns. Pure C, unit-tested on the PC.
 *
 * Two quick turns (a double tap) make a U-turn. Taken instantly, the cycle moved only one
 * frame's travel (7 units at cruise 7) between them and came back right beside its own trail,
 * overlapping it: the cycle is 50 wide and the trail runs down its centre. Once trails can be
 * crashed into, that's suicide.
 *
 * So a turn is allowed only once the cycle has travelled TURN_GATE_MIN_GAP since the last one.
 * A press that comes too early isn't lost: it waits, and fires as soon as the gap is reached
 * (the player meant it). So a double tap is still a U-turn, just one cycle-width across, and
 * every turn that isn't the second of a very quick pair is still instant.
 *
 * It also kept the 1st person view whole: the trail is cut a little in front of the camera
 * (TRAIL_MIN_FORWARD, trail_pieces.h), and the screen's edge shows what's well to the side of
 * what's ahead. Running 10 beside a trail, its missing near end was on screen: a hole through
 * which the far side of the trail showed. (The cut is now 10, small enough for 25 to the side.)
 *
 * A stopped cycle (against the arena edge) can always turn: it isn't travelling, so it would
 * otherwise never reach the gap and be stuck facing the wall.
 *
 * One press waits at a time: a newer one replaces it (the latest is what the player wants now).
 */

#define TURN_GATE_MIN_GAP 50 // one cycle width: a U-turn leaves 25 units between cycle and trail

#define TURN_NONE 0
#define TURN_LEFT (-1)
#define TURN_RIGHT 1

typedef struct
{
	short travelled;	// since the last turn, capped at TURN_GATE_MIN_GAP
	signed char waiting; // a turn pressed too early (TURN_LEFT / TURN_RIGHT), or TURN_NONE
} turn_gate;

// Ready to turn straight away
void turn_gate_init(turn_gate *g);

// Each game frame, before the cycle moves: pressed is this frame's turn press (or TURN_NONE).
// Returns the turn to make now, or TURN_NONE.
int turn_gate_update(turn_gate *g, int pressed);

// Each game frame, after the cycle moves: how far it went (0 if it's stopped).
void turn_gate_moved(turn_gate *g, int distance);

#endif
