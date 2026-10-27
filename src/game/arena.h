#ifndef __ARENA_H__
#define __ARENA_H__

#include "world.h"

/*
 * Arena size, as runtime state so it can be tuned from the pad while playing.
 * Pure C, unit-tested on the PC.
 *
 * The arena is a square of walls centred on the world origin. Each side is made of
 * equal wall pieces, so the size is a whole number of pieces: half = pieces * PIECE_HALF.
 * Pieces keep a fixed width at every arena size: SGL drops a polygon that crosses the
 * camera's near plane, so wide pieces would pop out when the 1st person camera is close.
 */

#define ARENA_PIECE_HALF 125 // a wall piece is 250 units wide
#define ARENA_PIECES_MIN 8	 // 1000 half: about the old debug arena
#define ARENA_PIECES_DEFAULT 64 // 8000 half: 16000 across, 8x the old arena
#define ARENA_PIECES_MAX 112 // 14000 half: stays inside WORLD_COORD_LIMIT with room to spare
#define ARENA_PIECES_STEP 8
#define ARENA_WALL_CLEARANCE 96 // keeps the 1st person camera clear of the walls

// Level of detail for walls: pieces are 250 near the camera, then grouped into longer pieces
// with distance (far away they're a few pixels tall; fewer quads cost less CPU and VDP1 time).
// Every column stays on the 250 grid, so wall edge lines still meet floor lines.
#define ARENA_LOD_NEAR 3000		 // within this, every 250 piece is drawn
#define ARENA_LOD_MID 8000		 // within this, 4 pieces per quad (1000); beyond, 8 (2000)
#define ARENA_WALL_COLUMNS_MAX 64 // the most columns one side can need (~42 for the largest arena)

// Sets the pieces per side, clamped to MIN..MAX. Returns the value used.
int arena_set_pieces(int pieces);
int arena_pieces(void);
int arena_half(void);		  // centre to wall
int arena_player_limit(void); // furthest a bounded cycle may go from the centre, on each axis

/*
 * Which pieces of one side overlap the visible span [along_min, along_max] on its axis.
 * The side's columns start at side_start and run in direction dir (+1 or -1).
 * Returns the number of pieces to draw (0 if none) and sets *first.
 * (Whether the wall's line is in view at all is the caller's check.)
 */
int arena_visible_pieces(int side_start, int dir, int along_min, int along_max, int *first);

/*
 * The columns to draw for pieces [first, first + count) of one side, spaced by distance from the
 * eye: eye_along is its coordinate along the side, eye_across its distance from the wall's line.
 * Writes each column's coordinate along the side; the first and last are the run's ends.
 * Returns the number of columns (at most max_columns).
 */
int arena_wall_columns(int side_start, int dir, int first, int count, int eye_along, int eye_across,
					   short *along, int max_columns);

#endif
