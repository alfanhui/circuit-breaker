#ifndef __ROUNDS_H__
#define __ROUNDS_H__

/*
 * The arena's rounds (docs/STORY.md: "The arena's rounds"). Pure C, unit-tested on the PC.
 *
 * Five rounds, each one turn of the loop the CPU is stuck in. A round is won by being the last
 * one moving. Then, between rounds:
 *
 *   ROUND_SINKING  the walls sink into the floor (and every trail with them)
 *   ROUND_OPEN     no walls: you ride on over the endless floor for a few seconds
 *   ROUND_RISING   the next round's walls rise round you
 *
 * and the next round is played. After the last round it starts again from the first: the loop
 * goes round (until the designed crash breaks it: not built yet).
 *
 * Losing a round (you crash) isn't handled here: the terminal screen (game_state.h) restarts the
 * round you were on, as this keeps its number.
 *
 * rounds_update reports the moments the game has to act on, as events: when the walls start to
 * sink (nothing left to hit: you become a ghost), when they're about to rise (with nothing
 * standing, the world can be moved unseen: input.c slides it so you're near the centre again),
 * and when they're up (the round's rivals come in and you're solid again).
 */

#define ROUND_COUNT 5

#define ROUND_SINK_SHIFT 6	   // the walls sink in 2^6 = 64 frames, about 1 s
#define ROUND_OPEN_FRAMES 180  // 3 s with no walls
#define ROUND_RISE_SHIFT 6	   // and rise in 64 frames
#define ROUND_WALLS_FULL 256   // the walls' height, x256: full

// Phases
#define ROUND_PLAYING 0
#define ROUND_SINKING 1
#define ROUND_OPEN 2
#define ROUND_RISING 3

// Events: what rounds_update says happened this frame
#define ROUND_NOTHING 0
#define ROUND_CLEARED 1 // the last rival is gone: the walls start to sink
#define ROUND_RISES 2	// the walls are about to rise: move the world now, if it needs moving
#define ROUND_BEGINS 3	// the next round's walls are up: bring its rivals in

typedef struct
{
	unsigned char number;  // 0 to ROUND_COUNT - 1 (shown to the player as 1 to 5)
	unsigned char phase;   // ROUND_PLAYING ... ROUND_RISING
	unsigned short frames; // frames since this phase began
} rounds;

// The first round, being played
void rounds_init(rounds *r);

// The current round again from its start, being played (after a crash: the terminal screen)
void rounds_restart(rounds *r);

// Each game frame. rivals_alive: how many of this round's rivals are still riding. Returns an event
int rounds_update(rounds *r, int rivals_alive);

// How far the arena's walls stand, 0 (flat in the floor) to ROUND_WALLS_FULL
int rounds_wall_rise(const rounds *r);

// How many rivals the current round has (STORY.md's table: from round 4 one is the Watchdog)
int rounds_rivals(const rounds *r);

// What the status line says as the round begins: "Loop iteration 1" and so on
const char *rounds_title(const rounds *r);

#endif
