#ifndef __CITY_H__
#define __CITY_H__

#include "corridor.h"

/*
 * Work RAM's city (docs/WORK_RAM.md, "The city"): its first slice, the entry blocks. Pure C,
 * unit-tested on the PC; level.c draws the towers as the grey box's boxes.
 *
 * The floor plan is tools/work_ram_plan.py's map (docs/images/work_ram_plan.svg): one character a
 * tower, 500 x 500 (11.5 m), in blocks of 2 x 2 that touch, with streets 200 wide (4.6 m) between
 * the blocks. Below is the corner of it round the entry: the plan's columns 0-2 (east, out from the
 * corridor) and rows 3-9 (south, across it):
 *
 *            col 0      col 1      col 2
 *   row 3    guard      guard      guard
 *   row 4    -60        -100       guard
 *   row 5    ENTRY 0    -50        guard        <- the corridor comes in from the west
 *            ~~~~~~~~~~ street ~~~~~~~~~~
 *   row 6    -60        -110       guard
 *   row 7    -100       -160       guard
 *            ~~~~~~~~~~ street ~~~~~~~~~~
 *   row 8    -160       -210       guard
 *   row 9    -200       -260   |   -310
 *
 * The corridor's far end opens onto the entry's roof, level with its floor (0): the city falls
 * away below it. Each tower's lower the further you've walked from the entry, so the way on is
 * down: step off a lip onto the next tower in a block (50-60 down, a clean drop), or jump a street
 * at a full run (walker.h: a jump clears ~225). Within a block you can climb back up, at a run (up
 * to 100), so a drop isn't one way; across a street you can't. The guard pages stand 400 over the
 * entry, far too tall to stand on: walls that turn the way. (Elsewhere in the plan, sealed roofs
 * stand over 100 above all round them: out of reach.) Past the slice's edges and in the streets
 * there's nothing: a lip over a fall to nothing holds however long you push (walker.h), so you
 * can't fall off the slice.
 *
 * Positions as the grey box's (corridor.h): out from the arena's wall (the entry starts at
 * CORRIDOR_LENGTH, the corridor's far end) and across from the corridor's middle line, here in
 * world units (the boxes are in sixteenths of its width: the city's streets aren't). The plan's
 * x (east) is out, its y (south) is across, row 5's top edge on the middle line: the entry roof
 * fills the corridor's right half (its far wall is left only on the left half, behind the grey
 * box's tower)
 */

#define CITY_TOWERS 21
#define CITY_TOWER_SIZE 500
#define CITY_STREET 200 // between blocks: a jump at a full run (walker.h: one clears ~225)
#define CITY_GROUND (-3000) // far below the lowest roof (the plan's goes to -2400): the towers' feet

typedef struct
{
	short out_min, out_max;		  // from the arena's wall
	short across_min, across_max; // from the corridor's middle line
	short height;				  // its roof (0: the corridor's floor)
} city_tower;

extern const city_tower city_towers[CITY_TOWERS];

// A tower's two opposite corners, in the world's x and y
void city_tower_corners(const corridor *c, const city_tower *t, world_point *a, world_point *b);

// Adds the towers' roofs to where you can stand. Returns false if there wasn't room for them all
int city_walker_area_add(const corridor *c, walker_area *area);

#endif
