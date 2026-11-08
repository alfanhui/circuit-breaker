#ifndef __WALL_FLICKER_H__
#define __WALL_FLICKER_H__

#include "world.h"

/*
 * The flicker: a section of the arena's outer wall that glitches (docs/STORY.md: "The arena's
 * rounds"). Pure C, unit-tested on the PC; level.c draws it.
 *
 * The loop is a program stuck repeating itself, and from round 2 something in it is wrong: a few
 * of the wall's pieces don't hold. In bursts, some of them drop out for a frame or two (a hole:
 * you see out of the arena) or flash, the wall's glowing base stretched up their whole height.
 * Each round it's somewhere else, and it grows: round 2 three pieces now and then, round 5 four,
 * nearly all the time. Until round 5 it's only a picture: the section is as solid as the rest of
 * the wall (riding into it is a crash). Round 5's gives way: riding into it is the designed crash
 * (breach.h), and the section opens up into a hole you ride on through.
 *
 * Bursts are random, from the flicker's own xorshift sequence (as the bots' reaction times are):
 * each game step a burst starts with a chance of 1 in (chance + 1) for the round, chance being
 * 2^n - 1 (a mask and a compare: no division), and lasts 2 to 5 steps. On each of its steps, a
 * quarter of the pieces are hidden and a quarter flashed, picked from the random bits.
 *
 * Positions are along the side, from the arena's middle, on the walls' 250 grid (arena.h:
 * arena_grid_half): level.c draws the section's pieces one by one between the side's runs.
 * They're chosen to stay inside the square even when round 5 has closed in all the way (3000
 * from the middle), and clear of where the rooms' cross meets the walls (round 4)
 */

// The sides, as level.c's sides[] (world north is -y, east is -x: world.h)
#define WALL_FLICKER_NORTH 0
#define WALL_FLICKER_EAST 1
#define WALL_FLICKER_SOUTH 2
#define WALL_FLICKER_WEST 3

#define WALL_FLICKER_PIECES_MAX 4  // the widest section: a mask's bits, one per piece (3-4 wide reads
								   // as a gap you could aim for; wider is just a broken wall)
#define WALL_FLICKER_PIECE 250	   // a wall piece (2 x ARENA_PIECE_HALF)
#define WALL_FLICKER_HIT_MARGIN 125 // a hit counts this far past either end: half a piece of
									// forgiveness, for a cycle that clips the section's edge

typedef struct
{
	unsigned char count;  // pieces in the section; 0: no flicker this round
	unsigned char side;	  // WALL_FLICKER_NORTH ...
	short along_min;	  // where it starts along the side (x for north and south, y for east and west)
	unsigned char chance; // a burst starts when (random & chance) == 0: a mask, 2^n - 1
	unsigned char gives_way; // 1: riding into it is the designed crash (round 5), not a crash
	unsigned int random;  // its own random sequence (xorshift: never 0)
	unsigned char burst;  // steps left of the burst going on; 0: holding
	unsigned short hidden;	// this step: bit i set, piece i (from along_min) isn't drawn
	unsigned short flashed; // this step: bit i set, piece i is drawn as a glow (never also hidden)
} wall_flicker;

// The flicker for this round (0-based, as rounds.h's number): none in round 1. Starts holding
void wall_flicker_set_round(wall_flicker *f, int round);

// Each game step: a burst starts, goes on or ends, and the pieces it hits are picked
void wall_flicker_step(wall_flicker *f);

// Where the section ends along its side: along_min + count pieces
int wall_flicker_along_max(const wall_flicker *f);

// Whether a cycle stopped at at, facing heading, ran into the section: it faces the section's
// wall, it's up against it (at limit from the middle: arena_player_limit), and it's along the
// section (WALL_FLICKER_HIT_MARGIN either side). The sides are numbered as the headings are
int wall_flicker_hit(const wall_flicker *f, world_point at, int heading, int limit);

// The designed crash: every piece of the section gone, a hole (called each frame instead of
// wall_flicker_step, so no burst puts a piece back)
void wall_flicker_open(wall_flicker *f);

#endif
