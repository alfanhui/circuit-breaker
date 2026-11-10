#ifndef __CORRIDOR_H__
#define __CORRIDOR_H__

#include "world.h"
#include "wall_flicker.h"
#include "walker.h"
#include "target.h"

/*
 * The corridor behind the flicker (docs/STORY.md: the designed crash). Pure C, unit-tested on
 * the PC; level.c draws it.
 *
 * When round 5's flicker gives way (breach.h), there's something behind it: a corridor as wide
 * as the section, running straight out from the arena's wall, with walls either side and one
 * across its far end. Its floor isn't the arena's grid: just past the hole the grid tails off
 * under a floor of dark, matt glass hexagons, scattered at first, then whole. That's where you
 * come to a stop.
 *
 * Positions in it are (out, across): out from the arena's wall (0 at the wall), and across the
 * corridor (the same coordinate as along the flicker's side: x for north and south, y for east
 * and west). corridor_point turns them into the world's x and y.
 *
 * The hexagons point along out (corners at out +-R) and are flat across (sides at across +-H),
 * in columns CORRIDOR_HEX_STEP apart along out. A column's centres are 2H apart across, and
 * every other column's are moved H across, so they tile with no gaps. Each is cut to the
 * corridor (its walls, and the arena's wall at out 0) once, as it opens; drawing only cuts them
 * again to what the camera sees (corridor_clip).
 */

#define CORRIDOR_LENGTH 4500	 // out from the arena's wall to the far end's wall: whole 250 pieces
#define CORRIDOR_HEX_R 126		 // a hexagon's centre to its pointed corners: 252 across, a grid cell's size
#define CORRIDOR_HEX_H 109		 // its centre to its flat sides: R x sqrt(3) / 2
#define CORRIDOR_HEX_STEP 189	 // between columns, along out: 1.5 R
#define CORRIDOR_TAIL_COLUMNS 6	 // the first 6 columns are scattered, more laid in each (corridor_open)
#define CORRIDOR_TILES_MAX 160
#define CORRIDOR_TILE_CORNERS 8	 // a hexagon (6) cut by at most 2 of the corridor's sides (it's far
								 // narrower than the corridor): at most 8
#define CORRIDOR_CLIP_MAX 16	 // a tile cut again by the screen's 4 sides: at most 14
#define CORRIDOR_SHADES 3		 // three shades of glass, so no two neighbours are alike
#define CORRIDOR_SEAM_PARTS 16	 // each tile is shrunk a sixteenth towards its centre: the seams
								 // between them (the floor under them, level.c) show as lines
#define CORRIDOR_FACES 3		 // its walls' inner faces: either side, and the far end

typedef struct
{
	unsigned char count; // corners, in order round it
	unsigned char shade; // 0 .. CORRIDOR_SHADES - 1
	world_point centre;	 // the hexagon's: every corner is within CORRIDOR_HEX_R of it
	world_point corners[CORRIDOR_TILE_CORNERS];
} corridor_tile;

typedef struct
{
	unsigned char open;		  // 0: there's no corridor (until the flicker gives way)
	unsigned char side;		  // the arena wall it's behind: WALL_FLICKER_NORTH ... (as the headings)
	short across_min;		  // its side walls, across
	short across_max;
	short wall;				  // the arena's wall, from the middle (arena_half when it opened)
	unsigned char tile_count; // the floor's hexagons laid, in world x and y
	corridor_tile tiles[CORRIDOR_TILES_MAX];
} corridor;

void corridor_init(corridor *c); // closed

// Opens the corridor behind a flicker's section, with the arena's wall half from the middle:
// works out where it is and lays its floor (a few divisions, once, as it opens)
void corridor_open(corridor *c, const wall_flicker *f, int half);

// A point in the corridor, (out, across), in the world's x and y
world_point corridor_point(const corridor *c, int out, int across);

// How far out a point in the world is: out from the arena's wall, along the corridor (negative:
// still inside the arena)
int corridor_out(const corridor *c, world_point p);

// Its walls' inner faces, each a line on the floor from one end to the other: from[i] to to[i].
// Writes CORRIDOR_FACES
void corridor_faces(const corridor *c, world_point *from, world_point *to);

/*
 * Cuts a convex polygon (n corners in order round it, either way) to a convex quad (its corners
 * in order round it, either way): Sutherland-Hodgman, the polygon cut by each of the quad's
 * sides in turn. Writes what's left and returns how many corners (0: they don't meet). n is at
 * most CORRIDOR_TILE_CORNERS; out has room for CORRIDOR_CLIP_MAX. A polygon wholly inside costs
 * no division; each edge cut across costs one (64-bit: the screen's far corners are tens of
 * thousands of units off), so only the tiles at the screen's edges cost anything
 */
int corridor_clip(const world_point *poly, int n, const world_point quad[4], world_point *out);

/*
 * The grey box (docs/WORK_RAM.md): plain boxes standing on the corridor's floor towards its far
 * end, to try being on foot with heights (game/walker.h) before the city's built. Each is a roof
 * at a height, from out_min to out_max along the corridor, and across from across_min to
 * across_max sixteenths of its width (so it fits however wide the flicker made it). In order up
 * the corridor:
 *   - stairs up its left half: twenty steps, each 8 higher than the last (18 cm)
 *   - a platform across it, 160 up (3.7 m)
 *   - past it, on the left, a lower roof at 64: a clean drop (96), long enough to land a run's jump
 *   - on the right, the floor: a long drop (160), a stagger
 *   - beyond the lower roof, a tower too tall to climb: a wall
 *   - before the stairs, a crate waist-high (40: 92 cm) across the whole corridor: a wall
 *     walking, a vault over running
 *   - before that, from where you get up, the run's moves taught (walker.h): a rail to vault
 *     over and a duct to slide under, each across the whole corridor (no way round), then on the
 *     right half (across 8 .. 16, which is on your left as you face out: the halves go by across,
 *     not by your view) a ledge to climb (90) beside one too high to (140)
 */
#define CORRIDOR_BOXES 29

typedef struct
{
	short out_min, out_max;
	unsigned char across_min, across_max; // sixteenths of the corridor's width
	short height;
	short floor; // its bottom: 0 stands on the floor; higher, it's a bar you can pass under
} corridor_box;

extern const corridor_box corridor_boxes[CORRIDOR_BOXES];

// A box's two opposite corners, in the world's x and y
void corridor_box_corners(const corridor *c, const corridor_box *box, world_point *a, world_point *b);

// Where you can stand on foot (walker.h): the corridor's floor, walled in (its sides, its far end's
// left half, and the hole behind you: you can't step back into the arena, its trails stand there),
// the grey box's roofs on it, and past the far end's right half the city's towers (city.h)
void corridor_walker_area(const corridor *c, walker_area *area);

// What the head can lock onto on foot (target.h): the yard's rail, duct and ledge, the crate, the
// top of the stairs, the drop's landing, the tower and the doorway out. No enemy yet (the drone)
void corridor_targets(const corridor *c, target_list *list);

#endif
