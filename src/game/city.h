#ifndef __CITY_H__
#define __CITY_H__

#include "corridor.h"
#include "drone.h"

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
 *   row 0  -1460  CD     -1400  -1310   ##    (s)    (s)      CD: the CD block's hub, -1400
 *   row 1  -1400  -1360  -1360  -1260   ##    (s)     ##
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
#define CITY_HUB_COLUMN 1 // the CD block's hub: the end of the way down
#define CITY_HUB_ROW 0
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

/*
 * The CD block itself: a disc in the sky, over the city (docs/STORY.md: "the CD block as a vast
 * disc"; the areas are seen from the hub long before they're reached). 7000 across: from the
 * arena it stands over the wall the corridor opens in, from the city it's most of the sky.
 *
 * Under it, the drive's works: the spindle under the hole in its middle, and the laser's pickup
 * on its rail, reading the disc from below. The pickup is over the hub: so the disc's middle is
 * CITY_DISC_OFFSET across from the hub's (a pickup reads the tracks, not the hole). The hub's lift
 * (game/lift.h) goes up to the pickup's underside from a pad at the middle of the hub's roof: the
 * way into the CD block, when it's built, is in through the pickup
 */
#define CITY_DISC_HEIGHT 4000 // above the corridor's floor: 5400 over the hub's roof
#define CITY_DISC_RADIUS 3500 // across its corners (an octagon)
#define CITY_DISC_HOLE 500	  // the hole in its middle, the spindle's
#define CITY_DISC_OFFSET 1500 // its middle from the hub's, across (south, over the city)
#define CITY_PICKUP_HALF 300  // the pickup: a box 600 square, hung under the disc over the hub
#define CITY_PICKUP_BOTTOM 3300
#define CITY_PICKUP_TOP 3650
#define CITY_LIFT_TOP (CITY_PICKUP_BOTTOM - 110) // where the lift stops: your eye 75 under the pickup
#define CITY_LIFT_PAD 60 // half the pad's width

// A point from the middle of the hub's roof (out and across from it), for a corridor behind
// `side`'s wall, `wall` from the arena's middle, whose middle line is `middle` across: so the hub
// and what's over it can be placed from the arena too, before the corridor's open
world_point city_hub_point(int side, int wall, int middle, int out, int across);
// The middle of the hub's roof itself (the lift's pad)
world_point city_hub_middle(int side, int wall, int middle);

/*
 * What's seen from each roof: worked out ahead (tools/make_city_seen.py writes city_seen.c; it
 * has the rule), not every frame. VDP1 has no depth buffer, so a tower hidden behind another is
 * still filled, and deep in the city most are hidden. What hides what depends only on where you
 * stand, and the city doesn't change: so for each roof, cut into CITY_SEEN_PATCHES each way (a
 * whole roof is too wide for one answer), there's a bit for every tower that can be seen from
 * somewhere on that patch, with the eye up to CITY_SEEN_EYE over the roof. 3.7 KB.
 * level.c leaves out the towers a patch can't see with one bit test each, and only picks hiders
 * for the rest (game/skyline.h). Off a roof (in the air, on the lift) there's no table: all 84
 */
#define CITY_SEEN_PATCHES 2
#define CITY_SEEN_EYE 200
#define CITY_SEEN_BYTES ((CITY_TOWERS + 7) >> 3)
extern const unsigned char city_seen[CITY_TOWERS][CITY_SEEN_PATCHES * CITY_SEEN_PATCHES][CITY_SEEN_BYTES];

/*
 * And from inside the corridor: which towers can be seen through its doorway (city_seen_doorway,
 * in city_seen.c too). Every line of sight from in there to the city passes through the doorway
 * (level.c cuts the city to its two sides), so a tower that one hider hides from every point of
 * the doorway is hidden from everywhere inside. The places a hider hides a tower from are convex
 * (make_city_seen.py has why), so the doorway's two ends are enough, as a patch's four corners
 * are. The doorway's the far end's right half, from the middle line across, as wide as the hole
 * in the arena's wall: at most CITY_DOORWAY_WIDE (wall_flicker.h: 4 pieces of 250, halved), and a
 * narrower one sees no more. The eye: up to CITY_DOORWAY_EYE (the platform's top, 160, and
 * CITY_SEEN_EYE). Worked out in the game it was ~60 ms of the SH-2, at the breach
 */
#define CITY_DOORWAY_WIDE 500
#define CITY_DOORWAY_EYE (160 + CITY_SEEN_EYE)
extern const unsigned char city_seen_doorway[CITY_SEEN_BYTES];

// Which of a roof's patches a point on it is in (out and across as the tower's own)
int city_seen_patch(const city_tower *roof, int out, int across);
// Whether `tower` can be seen from patch `patch` of roof `from` (both as city_towers' index)
int city_sees(int from, int patch, int tower);
// The roof you're stood on at (out, across) with your eye at z, as city_towers' index: -1 if
// you're not on one, or higher over it than the table allows for (a jump's fine, the lift's not)
int city_roof_under(int out, int across, int z);

// Adds the towers' roofs to where you can stand. Returns false if there wasn't room for them all
int city_walker_area_add(const corridor *c, walker_area *area);

/*
 * The watchdog's drone (game/drone.h): its first route. It rides the street between the plan's
 * columns 1 and 2, beside the guard pages of column 2, facing back towards the corridor over the
 * roofs of columns 0 and 1: the way down. Three stops down the street as the roofs step down, by
 * rows 6-7, 8-9 and 10-11, each 300-400 over the roofs it watches, its scan angled down at them
 * (drone.h), waiting CITY_DRONE_WAIT at each, then back up. You go down past it.
 * Writes CITY_DRONE_STOPS stops (in the world's x and y: for a corridor behind `c`'s wall)
 */
#define CITY_DRONE_STOPS 4
#define CITY_DRONE_WAIT 180 // 3 s at a stop
#define CITY_DRONE_IN 40	  // its middle this much in from the street's (drone_draw.c: it's 270 long)
void city_drone_route(const corridor *c, drone_stop *route);

// Every tower as a box from the ground to its roof, for the drone's sight (drone_sees): CITY_TOWERS
void city_drone_boxes(const corridor *c, drone_box *boxes);

#endif
