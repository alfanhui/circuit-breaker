#include "test.h"
#include "../src/game/turn_gate.h"

// One game frame: the press (if any), then the cycle moves `speed`. Returns the turn made
static int frame(turn_gate *g, int pressed, int speed)
{
	int turn = turn_gate_update(g, pressed);
	turn_gate_moved(g, speed);
	return turn;
}

static void first_turn_is_instant(void)
{
	turn_gate g;

	turn_gate_init(&g);
	CHECK_EQ(TURN_LEFT, frame(&g, TURN_LEFT, 7));
}

static void turns_far_apart_are_instant(void)
{
	turn_gate g;

	turn_gate_init(&g);
	frame(&g, TURN_LEFT, 7);
	for (int i = 0; i < 10; ++i) // 70 units: past the gap
		frame(&g, TURN_NONE, 7);
	CHECK_EQ(TURN_RIGHT, frame(&g, TURN_RIGHT, 7));
}

static void double_tap_waits_for_the_gap_and_is_not_lost(void)
{
	turn_gate g;
	int frames = 0;

	turn_gate_init(&g);
	frame(&g, TURN_LEFT, 7);
	CHECK_EQ(TURN_NONE, frame(&g, TURN_LEFT, 7)); // the second tap, one frame later: too soon...
	++frames;
	while (frame(&g, TURN_NONE, 7) == TURN_NONE && frames < 100) // ...but it waits
		++frames;
	// It fires once 50 units are behind: 8 frames of 7 after the first turn (56), so at most
	// 7 frames (0.12 s) after the second tap
	CHECK_EQ(7, frames);
}

static void waiting_turn_fires_once(void)
{
	turn_gate g;

	turn_gate_init(&g);
	frame(&g, TURN_LEFT, 7);
	frame(&g, TURN_RIGHT, 50); // too soon, waits; this frame covers the gap
	CHECK_EQ(TURN_RIGHT, frame(&g, TURN_NONE, 7));
	for (int i = 0; i < 20; ++i)
		CHECK_EQ(TURN_NONE, frame(&g, TURN_NONE, 7));
}

static void latest_press_wins(void)
{
	turn_gate g;

	turn_gate_init(&g);
	frame(&g, TURN_LEFT, 7);
	frame(&g, TURN_LEFT, 7);  // waits...
	frame(&g, TURN_RIGHT, 7); // ...replaced: the player changed their mind
	for (int i = 0; i < 10; ++i)
	{
		int turn = frame(&g, TURN_NONE, 7);
		if (turn != TURN_NONE)
		{
			CHECK_EQ(TURN_RIGHT, turn);
			break;
		}
	}
}

static void stopped_cycle_can_always_turn(void)
{
	turn_gate g;

	turn_gate_init(&g);
	frame(&g, TURN_LEFT, 7);
	frame(&g, TURN_NONE, 0); // against the arena edge: not moving
	CHECK_EQ(TURN_RIGHT, frame(&g, TURN_RIGHT, 0)); // not stuck facing the wall
}

static void fast_cycle_reaches_the_gap_sooner(void)
{
	turn_gate g;

	turn_gate_init(&g);
	frame(&g, TURN_LEFT, 40);					  // top cruise speed
	CHECK_EQ(TURN_NONE, frame(&g, TURN_LEFT, 40)); // 40 behind: not yet...
	CHECK_EQ(TURN_LEFT, frame(&g, TURN_NONE, 40)); // ...80: turns the next frame
}

void run_turn_gate_tests(void)
{
	RUN(first_turn_is_instant);
	RUN(turns_far_apart_are_instant);
	RUN(double_tap_waits_for_the_gap_and_is_not_lost);
	RUN(waiting_turn_fires_once);
	RUN(latest_press_wins);
	RUN(stopped_cycle_can_always_turn);
	RUN(fast_cycle_reaches_the_gap_sooner);
}
