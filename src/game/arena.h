#ifndef __ARENA_H__
#define __ARENA_H__

#include <stdbool.h>
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

/*
 * Layouts: what stands inside the square (docs/STORY.md: "The arena's rounds"). Each is a few
 * blocks: axis-aligned rectangles of wall, as tall as the outer walls, on the floor's grid.
 * A block is the same shape a trail segment fattened by its clearance is (collision.h), so the
 * same test stops a cycle at one. Positions scale with the arena: they're whole numbers of a
 * unit, an eighth of the centre-to-wall distance (1000 at the default size), so the square is
 * 16 units across:
 *
 *   ARENA_OPEN     nothing: the square alone (rounds 1 and 5)
 *   ARENA_DONUT    one big block in the middle, 6 units across: you ride round it, in a lane
 *                  5 units wide, never able to see across
 *   ARENA_PILLARS  four pillars, 3 units square, round the middle: a wide cross of open floor
 *                  between them and lanes round the outside
 *   ARENA_ROOMS    a thin wall cross from wall to wall makes four rooms, with a door 2 units
 *                  wide half way along each of its four arms
 *
 * Each layout also says where you start: somewhere clear, as the middle isn't always.
 */
#define ARENA_OPEN 0
#define ARENA_DONUT 1
#define ARENA_PILLARS 2
#define ARENA_ROOMS 3
#define ARENA_LAYOUT_COUNT 4
#define ARENA_BLOCKS_MAX 6			 // the rooms' cross: each of its lines in 3 pieces, cut by 2 doors
#define ARENA_INNER_WALL_HALF 125	 // the rooms' walls: one wall piece (250) thick
#define ARENA_UNIT_MIN (2 * ARENA_PIECE_HALF) // smaller (a tiny debug arena), the layout isn't laid out

// Sets the pieces per side, clamped to MIN..MAX. Returns the value used. The layout's blocks are
// laid out again for the new size
int arena_set_pieces(int pieces);
int arena_pieces(void);
int arena_half(void);		  // centre to wall (the inset taken off)
int arena_player_limit(void); // furthest a bounded cycle may go from the centre, on each axis

/*
 * The walls pulled in, by `units` on every side, from the square the pieces make: the last round
 * closing in (rounds.h), smoothly, a unit at a time. Clamped so ARENA_INSET_LEAVES is left from
 * the middle to each wall. Returns the inset used; 0 puts the walls back.
 * The walls' columns stay on a 250 grid from the middle (arena_grid_half): only the piece at each
 * end of a side is cut short, so a wall's texture doesn't crawl along it as it slides in
 */
#define ARENA_INSET_LEAVES 1000
int arena_set_inset(int units);
int arena_inset(void);
// The walls' first column, from the middle: arena_half rounded up to whole 250 pieces. A side's
// columns run from -arena_grid_half to +arena_grid_half, cut back to the corners (level.c)
int arena_grid_half(void);
// How many 250 pieces a side's grid has: 2 * arena_grid_half / 250, kept with it (no division to draw)
int arena_grid_pieces(void);

// Which layout stands inside the square: ARENA_OPEN (the default) ... ARENA_ROOMS
void arena_set_layout(int layout);
int arena_layout(void);
// The layout's blocks, for this arena size; *count gets how many (0 for ARENA_OPEN)
const world_rect *arena_blocks(int *count);
// The layout's unit: an eighth of arena_half, rounded down to whole wall pieces (0: too small for one)
int arena_unit(void);
// Where a rider starts this layout (clear of every block), and which way it faces
world_point arena_start(void);
int arena_start_heading(void);
/*
 * Where a rival starts, if the layout has places of its own for them (returns false if not: they
 * start round you, input.c). The rooms: you're in the south-west one, and the three rivals
 * (which: 0 .. 2) each have one of the others, at its middle. Everyone sets off through a door
 * into the next room round, the same way round as you (you ride north into the north-west room,
 * its rider east into the north-east, and so on): nobody meets head on in a doorway, and the room
 * you ride into has just been left
 */
#define ARENA_ROOM_RIVALS 3
bool arena_rival_start(int which, world_point *at, int *heading);
// p moved, if it has to be, so it's at least margin inside the walls and margin clear of every
// block: for placing riders. Out of a block the shortest way
world_point arena_clear_spot(world_point p, int margin);

/*
 * Which pieces of one side overlap the visible span [along_min, along_max] on its axis.
 * The side's columns start at side_start (-dir * arena_grid_half) and run in direction dir (+1 or -1).
 * Returns the number of pieces to draw (0 if none) and sets *first.
 * (Whether the wall's line is in view at all is the caller's check.)
 */
int arena_visible_pieces(int side_start, int dir, int along_min, int along_max, int *first);
// The same for a run of `pieces` pieces rather than a whole side: one face of a block
int arena_visible_pieces_of(int side_start, int dir, int pieces, int along_min, int along_max, int *first);

/*
 * The columns to draw for pieces [first, first + count) of one side, spaced by distance from the
 * eye: eye_along is its coordinate along the side, eye_across its distance from the wall's line.
 * Writes each column's coordinate along the side; the first and last are the run's ends.
 * Returns the number of columns (at most max_columns).
 */
int arena_wall_columns(int side_start, int dir, int first, int count, int eye_along, int eye_across,
					   short *along, int max_columns);

#endif
