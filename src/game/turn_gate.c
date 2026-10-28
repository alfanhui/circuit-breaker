#include "turn_gate.h"

void turn_gate_init(turn_gate *g)
{
	g->travelled = TURN_GATE_MIN_GAP;
	g->waiting = TURN_NONE;
}

int turn_gate_update(turn_gate *g, int pressed)
{
	int turn;

	if (pressed != TURN_NONE)
		g->waiting = (signed char)pressed; // the latest press is the one that counts
	if (g->waiting == TURN_NONE || g->travelled < TURN_GATE_MIN_GAP)
		return TURN_NONE; // nothing to do, or too soon: a press keeps waiting

	turn = g->waiting;
	g->waiting = TURN_NONE;
	g->travelled = 0;
	return turn;
}

void turn_gate_moved(turn_gate *g, int distance)
{
	if (distance <= 0)
	{
		g->travelled = TURN_GATE_MIN_GAP; // stopped: free to turn away
		return;
	}
	g->travelled = (short)(g->travelled + distance > TURN_GATE_MIN_GAP ? TURN_GATE_MIN_GAP : g->travelled + distance);
}
