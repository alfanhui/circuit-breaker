#ifndef __ATTRACT_TEXT_H__
#define __ATTRACT_TEXT_H__

#include "world.h"

/*
 * The prologue's loop writes "SEGA SATURN SH-2" across the floor with the cycle's trail, one
 * letter after another, for ever (docs/STORY.md). This builds the route that does it: a list of
 * waypoints the cycle rides between, each saying whether the trail is laid on the way there
 * (pen down) or not (pen up: it rides without one, from one stroke to the next). Pure C,
 * unit-tested on the PC. Built once, at start up.
 *
 * The font: each letter is a few strokes on a grid 6 wide and 10 tall, straight lines only
 * (trails can't go diagonally): a blocky, digital look. N is the pixel-font zig-zag, R is a P
 * with a stepped leg. Letters are 9 apart (6 and a gap of 3), a space another 6.
 *
 * In the world, a grid unit is ATTRACT_TEXT_UNIT. The spectator camera faces north, so the
 * text reads left to right going east (-x), and a letter's top is its far end (north, -y).
 *
 * The cycle can't turn back on itself in one go (a U-turn is two turns, turn_gate.h), and the
 * strokes are drawn so none needs to. Between strokes (pen up), where the way on would be back
 * the way it came, the path steps 1 unit to the side first: a jog nobody sees, with no trail.
 *
 * It ends one ATTRACT_TEXT_PERIOD east of where it began, riding the same way: put the cycle
 * back by exactly that, and it's the start again. The period is a whole number of the spectator
 * floor's 1000 unit tiles, so with nothing left standing the jump can't be seen.
 */

#define ATTRACT_TEXT "SEGA SATURN SH-2"
#define ATTRACT_TEXT_UNIT 100		  // world units per font grid unit: letters 600 x 1000
#define ATTRACT_TEXT_PERIOD 15000	  // a whole message, plus room for the last word to sink
// A spelt word sinks into the floor a unit every this many frames: half a dead rider's speed
// (RIDER_SINK_SPEED, rider.h), so the words linger on the spectator's view. 2 s for a trail
#define ATTRACT_SINK_FRAMES 2
// How long the message takes to write, ridden as the game rides it (every message after the
// first; test_attract_route.c measures it, and fails if the font, route or speeds change it), and
// so the steady speed the spectator camera pans at: a period in that many frames, x256. The
// phase: how far west of that steady point the cycle is on average (the pan aims at it)
#define ATTRACT_TEXT_FRAMES 5301 // 1 min 28 s
#define ATTRACT_TEXT_PAN_X256 724 // 15000 x 256 / 5301: 2.83 units a frame
#define ATTRACT_TEXT_PAN_PHASE 601 // the cycle keeps within 1365 of it either way
#define ATTRACT_TEXT_MAX_WAYPOINTS 256 // enough for the message (~170), with room to spare

typedef struct
{
	world_point p;
	unsigned char pen;		// 1: the trail is laid on the leg arriving here
	unsigned char word_end; // 1: the last stroke of a word ends here (the word can sink now)
} attract_waypoint;

// Builds the route for text. origin: where the first letter's bottom left corner goes, in the
// world. The cycle starts at waypoints[0] (a little before the first letter), riding towards
// waypoints[1]; the last waypoint is waypoints[0] moved ATTRACT_TEXT_PERIOD east (-x).
// Returns how many waypoints it wrote (at most max), 0 if they didn't fit.
int attract_text_build(const char *text, world_point origin, attract_waypoint *waypoints, int max);

#endif
