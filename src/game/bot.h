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
 * Step 1 of the plan (issue #37): drive straight, and when something comes within its
 * look-ahead, turn to whichever side has more room. No boost yet, no reaction delay yet.
 */

// How far ahead it looks, in frames of travel at cruise speed: 30 frames is half a second,
// 210 units at cruise 7. Measured in frames so it reacts as early at any cruise speed
#define BOT_LOOK_FRAMES 30
// How far it looks to each side to choose a way out: further than ahead, to judge which side
// opens up rather than which wall is a little further
#define BOT_SIDE_LOOK_FRAMES 120

// This frame's buttons for the bot riding me. trails: every trail on the field, its own included
// (the same list its cycle collides with)
rider_controls bot_think(const rider *me, const trail *const *trails, int trail_count);

#endif
