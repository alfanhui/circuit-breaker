#ifndef __CITY_H__
#define __CITY_H__

#include "corridor.h"

/*
 * Work RAM's city (docs/WORK_RAM.md, "The city"): the entry and the way down to the first hub, the
 * CD block. Pure C, unit-tested on the PC; level.c draws the towers as the grey box's boxes.
 *
 * The floor plan is tools/work_ram_plan.py's map (docs/images/work_ram_plan.svg): one character a
 * tower, 500 x 500 (11.5 m), in blocks of 2 x 2 that touch, with streets 200 wide (4.6 m) between
 * the blocks. Built: the plan's columns 0-6 (east, out from the corridor) and rows 0-11 (south,
 * across it), its whole low bank but the column by the channel. The table in city.c is the plan's
 * own (python tools/work_ram_plan.py --c prints it), so its heights are the ones the plan checks:
 *
 *            0      1      2      3      4      5      6
 *   row 0  -1460  CD     -1350  -1310   ##    (s)    (s)      CD: the CD block's hub, -1400
 *   row 1  -1400  -1360  -1300  -1260   ##    (s)     ##
 *   row 2   (s)    ##     ##    -1210   ##    (s)   -1150
 *   row 3    ##    ##     ##    -1160   ##    (s)   -1100
 *   row 4   -60   -100    ##    -1100  (s)    (s)     ##
 *   row 5  ENTRY  -50     ##    -1050   ##     ##     ##      <- the corridor comes in from the west
 *   row 6   -60   -110    ##    -1000  -950   -900   -860
 *   row 7  -100   -160    ##     ##     ##     ##    -810
 *   row 8  -160   -210    ##    (s)    -660   -700   -750
 *   row 9  -200   -260   -310    ##    -600    ##    -810
 *   row 10 -260    ##    -360    ##    -560    ##    (s)
 *   row 11 -300   -350   -400   BR     -510    ##    (s)      BR: Backup RAM's hub, -450
 *   (## a guard page, 400 up: a wall; (s) sealed, out of reach above all round it)
 *
 * The corridor's far end opens onto the entry's roof, level with its floor (0): the city falls
 * away below it, and every way on is down. The way to the CD block, 28 towers, winds: south down
 * columns 0-1, over to Backup RAM's block (its hub's on the way: gated, for later), east and up
 * column 6, west along row 6, then north up column 3 between the guard pages and west along row 0
 * to the hub (the unit test walks it: test_city.c). Step off a lip onto the next tower in a block
 * (up to 120 down, a clean drop), or jump a street at a run (walker.h: a jump clears ~225). Within
 * a block you can climb back up, at a run (up to 100); across a street you can't. Past the slice's
 * edges and in the streets there's nothing: a lip over a fall to nothing holds however long you
 * push (walker.h), so you can't fall off the slice.
 *
 * Positions as the grey box's (corridor.h): out from the arena's wall (the entry starts at
 * CORRIDOR_LENGTH, the corridor's far end) and across from the corridor's middle line, here in
 * world units (the boxes are in sixteenths of its width: the city's streets aren't). The plan's
 * x (east) is out, its y (south) is across, row 5's top edge on the middle line: the entry roof
 * fills the corridor's right half (its far wall is left only on the left half, behind the grey
 * box's tower)
 */

#define CITY_COLUMNS 7
#define CITY_ROWS 12
#define CITY_TOWERS (CITY_COLUMNS * CITY_ROWS)
#define CITY_ENTRY_ROW 5 // the entry's at column 0 of this row
#define CITY_TOWER_SIZE 500
#define CITY_STREET 200 // between blocks: a jump at a full run (walker.h: one clears ~225)
#define CITY_GROUND (-3000) // far below the lowest roof (the plan's goes to -2400): the towers' feet

// What a tower is, from the plan's map
#define CITY_ROOF 0	  // one you can stand on
#define CITY_GUARD 1  // a guard page: far too tall, a wall
#define CITY_SEALED 2 // sealed: higher than all round it, out of reach
#define CITY_HUB 3	  // a hub: an area's way in, and its lift. Lit, to be seen from afar (level.c)

typedef struct
{
	short out_min, out_max;		  // from the arena's wall
	short across_min, across_max; // from the corridor's middle line
	short height;				  // its roof (0: the corridor's floor)
	unsigned char kind;			  // CITY_ROOF ...
} city_tower;

extern const city_tower city_towers[CITY_TOWERS]; // row by row, CITY_COLUMNS to a row

// The tower at a column and row of the plan (city.h's map)
const city_tower *city_tower_at(int column, int row);

// A tower's two opposite corners, in the world's x and y
void city_tower_corners(const corridor *c, const city_tower *t, world_point *a, world_point *b);

// Adds the towers' roofs to where you can stand. Returns false if there wasn't room for them all
int city_walker_area_add(const corridor *c, walker_area *area);

#endif
