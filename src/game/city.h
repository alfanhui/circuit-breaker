#ifndef __CITY_H__
#define __CITY_H__

#include "corridor.h"
#include "drone.h"

/*
 * Work RAM's city: Low Work RAM, the bank the corridor opens into (docs/WORK_RAM.md, "A bigger
 * city"). Pure C, unit-tested on the PC; level.c draws the towers as the grey box's boxes.
 *
 * Low Work RAM is 1 MB at 0x0020_0000: 16 x 16 towers, each a page of 4 KB. The tower at row r,
 * column c is the page at 0x0020_0000 + r x 0x1_0000 + c x 0x1000 (city_tower_address). The floor
 * plan is tools/ram_banks_plan.py's map (docs/images/ram_banks_plan.svg, the _low.png close up):
 * one character a tower, 500 x 500 (11.5 m), in blocks of 2 x 2 that touch, with streets 200 wide
 * (4.6 m) between the blocks. The table in city.c is the plan's own (python
 * tools/ram_banks_plan.py --quick --c prints it), so the heights are the ones the plan checks.
 *
 * The layout: lanes a tower wide, walled by guard pages (far above the roofs), a maze on the towers
 * at even columns and rows: T-junctions, crossroads, dead ends and loops, to lose a watchdog round.
 * The corridor opens onto column 0, row 8 (the middle of the edge nearest the arena: the arena's
 * pyramid reaches the ground just where the corridor ends, so the city can't go round it). From
 * there the roofs fall away a block at a time: every street is level (a jump either way), and each
 * block steps down 90 on its far side (a clean drop out, a climb back in: walker.h climbs up to
 * 100). So every move can be made both ways. The far corner's at -630.
 *
 * Three of the six ways out are here, where the Saturn has them round Low Work RAM (the rest are
 * in High Work RAM, across the arena: planned, not built):
 *   Backup RAM (0x0018_0000, just below the bank): row 0, column 4
 *   the CD block (0x0580_0000, above it):          row E, column A, its lift and the disc over it
 *   the SCSP (0x05A0_0000, above that):            row E, column E
 * And two data buses' ends, to High Work RAM round the pyramid (planned): column 0, rows 0 and E.
 * Step off a lip onto the next tower in a block, or jump a street at a run (walker.h: a jump
 * clears ~225). Past the bank's edges and in the streets there's nothing: a lip over a fall to
 * nothing holds however long you push (walker.h), so you can't fall off it.
 *
 * Positions as the grey box's (corridor.h): out from the arena's wall (column 0 starts at
 * CORRIDOR_LENGTH, the corridor's far end) and across from the corridor's middle line, in world
 * units. The plan's x (columns) is out, its y (rows) across, row 8's top edge on the middle line:
 * the entry roof fills the corridor's right half (its far wall is left only on the left half)
 *
 *   out of column c:   CORRIDOR_LENGTH + 500 c + 200 (c / 2)            (to 500 more)
 *   across of row r:   500 r + 200 (r / 2) - 4800                       (to 500 more)
 */

#define CITY_COLUMNS 16
#define CITY_ROWS 16
#define CITY_TOWERS (CITY_COLUMNS * CITY_ROWS)
#define CITY_ENTRY_ROW 8  // the entry's at column 0 of this row
#define CITY_HUB_COLUMN 10 // the CD block's hub: its lift up to the disc
#define CITY_HUB_ROW 14
#define CITY_BACKUP_COLUMN 4 // Backup RAM's hub: the A-bus leaves from it (level.c, abus_add)
#define CITY_BACKUP_ROW 0
#define CITY_TOWER_SIZE 500
#define CITY_STREET 200 // between blocks: a jump at a full run (walker.h: one clears ~225)
#define CITY_GROUND (-3000) // far below the lowest roof (-630): the towers' feet
#define CITY_BASE 0x00200000 // Low Work RAM

// What a tower is, from the plan's map
#define CITY_ROOF 0	  // one you can stand on
#define CITY_GUARD 1  // a guard page: far too tall, a wall
#define CITY_SEALED 2 // sealed: higher than all round it, out of reach
#define CITY_HUB 3	  // a hub: an area's way in, and its lift. Lit, to be seen from afar (level.c)
// (No CITY_SEALED towers in Low Work RAM: every roof can be reached. Kept for the kind's value)

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

// Its address in Low Work RAM: the page it is (0x0020_0000 + row x 0x1_0000 + column x 0x1000)
unsigned int city_tower_address(int column, int row);

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
#define CITY_DISC_OFFSET (-1500) // its middle from the hub's, across: north, over the city (the hub's on its south edge)
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
 * somewhere on that patch, with the eye up to CITY_SEEN_EYE over the roof. 32 KB (256 towers).
 * The walls hide together (2026-11-16: occluder shrinking, the tool has how), and it's sure: no
 * line of sight reaches a tower it leaves out (tests/test_city_seen.c). From a roof patch a median
 * 71 of the 256 are left in.
 * level.c leaves out the towers a patch can't see with one bit test each, and only picks hiders
 * for the rest (game/skyline.h). Over a street there's its own table (city_seen_street, below); up
 * the lift, higher than any table allows for, there's none: all 256
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

/*
 * And over the streets (the user, 2026-11-18). Jumping a street you're over no roof, and with no
 * table every tower was drawn down to the ground: 606k pixels a frame mid-jump, against VDP1's
 * ~350k, so it couldn't finish 8-10 frames a jump and the roof ahead went missing
 * (city_traverse.steps). Worked out the same way (make_city_seen.py), for pieces of street: 250
 * along it (a roof patch's length) and its whole width (200) across, and each crossing whole.
 * The pieces, in order:
 *   0 .. 223    the streets between block columns (at one out each, running across): street s
 *               (after block column s, 0 .. 6) is s x 32 + its piece down it, from row 0
 *   224 .. 447  those between block rows (running out): 224 + s x 32 + its piece, from column 0
 *   448 .. 496  the crossings: 448 + 7 x (the row street) + (the column street)
 * The eye: as high as it gets over that piece (city_street_eye: a jump from the highest roof
 * beside it). Lower sees no more, as on a roof. 15.5 KB
 */
#define CITY_STREETS 7											  // between the 8 blocks, each way
#define CITY_STREET_ALONG (CITY_ROWS * CITY_SEEN_PATCHES)		  // pieces down a street: 32
#define CITY_STREET_PIECES (2 * CITY_STREETS * CITY_STREET_ALONG + CITY_STREETS * CITY_STREETS)
#define CITY_STREET_NO_EYE (-32768)								  // only guard pages beside it
extern const unsigned char city_seen_street[CITY_STREET_PIECES][CITY_SEEN_BYTES];
extern const short city_street_eye[CITY_STREET_PIECES];

/*
 * A tower's bit in any of these rows (the user, 2026-11-18: the boxes' reads). Not in the plan's
 * order: block by block, as level.c lists the boxes (2 x 2, the blocks row by row), each block's
 * four towers a nibble: (c, r), (c + 1, r), (c, r + 1), (c + 1, r + 1). So a block's seen or not
 * with one nibble, and a box's bit is its place in the list: no table from the box to its tower,
 * read ~600 times a frame. Shifts only (CITY_COLUMNS is a power of two)
 */
#define CITY_BLOCK_COLUMNS (CITY_COLUMNS >> 1)
int city_seen_bit(int tower);
// Whether a row (city_seen's, city_seen_street's, city_seen_doorway) has tower `tower` seen
int city_seen_has(const unsigned char *row, int tower);

// Which of a roof's patches a point on it is in (out and across as the tower's own)
int city_seen_patch(const city_tower *roof, int out, int across);
// Whether `tower` can be seen from patch `patch` of roof `from` (both as city_towers' index)
int city_sees(int from, int patch, int tower);
// The roof you're stood on at (out, across) with your eye at z, as city_towers' index: -1 if
// you're not on one, or higher over it than the table allows for (a jump's fine, the lift's not)
int city_roof_under(int out, int across, int z);
// The piece of street you're over at (out, across) with your eye at z (CITY_STREET_PIECES' order,
// above): -1 if you're not over a street, or higher than its eye (city_street_eye: the lift's
// over a roof, so it's never one). Once a frame, and only off a roof: compares, no division
int city_street_under(int out, int across, int z);

/*
 * Whether feet at world point p, height `feet` (as a roof's: walker_height), are on a hub's roof
 * (CITY_HUB). A hub's a safe place: no watchdog sees you there (the user, 2026-11-17: "The watch
 * dogs shouldnt be able to detect when on a hub"), as in its lift. Once a frame: a walk over the
 * towers, compares only
 */
bool city_on_hub(const corridor *c, world_point p, int feet);

// Adds the towers' roofs to where you can stand. Returns false if there wasn't room for them all
int city_walker_area_add(const corridor *c, walker_area *area);

/*
 * The watchdog's drones (game/drone.h): CITY_WATCHDOGS patrols on the way to the CD block (the
 * user, 2026-11-17: "about 3 before getting to CD Rom", the third round its lift). Each is a list
 * of stops in city.c's city_patrols, in the city's own frame (as the towers: out from the arena's
 * wall, across from the corridor's middle line), a height (as a roof's: 0 is the corridor's
 * floor), which way its sensor looks there (CITY_FACES_...: as the corridor runs, so a route reads
 * the same wherever the corridor opened) and how long it waits. Set CITY_WATCHDOGS to 0 and there's
 * no watchdog: it isn't flown, drawn or looked for, and nothing sees you
 */
#define CITY_WATCHDOGS 3
// As the plan's drawn (rows going down the page are its south). Not left and right: which side
// the rows grow to depends on which wall the corridor opened in (corridor_point_behind)
#define CITY_FACES_OUT 0   // away from the arena, the way the corridor runs (columns going up)
#define CITY_FACES_SOUTH 1 // towards row F
#define CITY_FACES_BACK 2  // back towards the arena
#define CITY_FACES_NORTH 3 // towards row 0
typedef struct
{
	short out, across;	  // where it stops, in the city's frame
	short height;		  // how high (a roof's frame: 0 the corridor's floor; the roofs are 0 .. -630)
	unsigned char faces;  // CITY_FACES_...
	unsigned short wait;  // frames it waits there (60 a second)
} city_drone_place;
// A patrol: its stops, and whether it goes back and forth along them (or round, a loop)
typedef struct
{
	const city_drone_place *places;
	unsigned char count; // up to DRONE_STOPS_MAX
	bool back_and_forth;
} city_patrol;
extern const city_patrol city_patrols[];
#define CITY_WATCHDOG (CITY_WATCHDOGS > 0)
// A CITY_FACES_ as the world's heading (HEADING_NORTH ...: world.h), for a corridor behind `c`'s wall
int city_faces_heading(const corridor *c, int faces);
// Patrol `patrol`'s route in the world's x and y, for a corridor behind `c`'s wall. Returns its
// count of stops
int city_drone_route(const corridor *c, int patrol, drone_stop *route);

// Every tower as a box from the ground to its roof, for the drone's sight (drone_sees): CITY_TOWERS
void city_drone_boxes(const corridor *c, drone_box *boxes);

#endif
