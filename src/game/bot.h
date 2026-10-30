#ifndef __BOT_H__
#define __BOT_H__

#include "rider.h"

/*
 * A computer rider's brain. Pure C, unit-tested on the PC.
 *
 * A bot never moves itself: each frame it decides which buttons to press (rider_controls), and
 * its rider is then driven exactly like a player's (rider.h). So it plays by the same rules.
 *
 * It only knows what it could see: it asks the same collision code the cycles use
 * (collision.h) how far it could go ahead, to the left and to the right. Nothing else: not
 * the player's buttons, not the whole map.
 *
 * It reacts like a person, not a machine. A person takes about 200 ms (12 frames) to react to
 * something they've seen, and not exactly the same time every time. So when something comes
 * into its look-ahead, the bot notices it, picks the side that had more room right then, and
 * presses the turn a reaction time later: what it saw, a moment ago. Each reaction time is
 * picked at random between BOT_REACTION_MIN and BOT_REACTION_MAX frames.
 *
 * Steps 1 and 2 of the plan (issue #37). No boost yet.
 */

// How far ahead it looks, in frames of travel at cruise speed: 30 frames is half a second,
// 210 units at cruise 7. Measured in frames so it reacts as early at any cruise speed. It must
// be well beyond the slowest reaction, or it would notice too late to turn
#define BOT_LOOK_FRAMES 30
// How far it looks to each side to choose a way out: further than ahead, to judge which side
// opens up rather than which wall is a little further
#define BOT_SIDE_LOOK_FRAMES 120

// Reaction time: 9 + (0..7) frames, so 9 to 16 (150 to 267 ms), 12.5 on average. The 7 is a
// mask, so picking it is a single AND: 8 values, a power of two
#define BOT_REACTION_MIN 9
#define BOT_REACTION_SPREAD_MASK 7
#define BOT_REACTION_MAX (BOT_REACTION_MIN + BOT_REACTION_SPREAD_MASK)

// What a bot remembers between frames
typedef struct
{
	unsigned int random;	// its own random number sequence (xorshift: never 0)
	unsigned char reacting; // frames until it presses the turn it has planned; 0: nothing seen
	signed char planned;	// the turn it will press: TURN_LEFT / TURN_RIGHT
} bot_mind;

// A fresh mind. seed: any number, picking its sequence of reaction times. The same seed always
// gives the same bot, so a run can be repeated exactly (tests, and the attract mode later)
void bot_init(bot_mind *mind, unsigned int seed);

// This frame's buttons for the bot riding me. trails: every trail on the field, its own included
// (the same list its cycle collides with)
rider_controls bot_think(bot_mind *mind, const rider *me, const trail *const *trails, int trail_count);

#endif
