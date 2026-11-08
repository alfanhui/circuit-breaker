#ifndef __SURROUND_H__
#define __SURROUND_H__

#include <stdbool.h>
#include "world.h"

/*
 * The raised ground round the arena: the arena is sunk into it, its walls the sides of the pit
 * and the surround's top level with theirs. Pure C, unit-tested on the PC; level.c draws it.
 *
 * It's a ring of flat rectangles, from the walls out to SURROUND_EXTENT: a thin rim along the
 * walls' tops (it glows, like their top edge), then the dark ground beyond. Nothing outside the
 * walls is the floor any more, so the floor's grid is only seen inside the arena.
 *
 *           +---------------- north body ----------------+
 *           +-------------- north rim -------------------+
 *           | west |  w  |                    |  e  | east |
 *           | body | rim |      the arena     | rim | body |
 *           |      |     |                    |     |      |
 *           +-------------- south rim -------------------+
 *           +---------------- south body ----------------+
 *   (x is west +, so west is on the left as seen from above with north up)
 *
 * The rectangles don't overlap (the north and south ones take the corners), so none has to be
 * drawn over another: they're all at one height.
 *
 * Drawing a rectangle thousands of units across from a camera above it: VDP1 wraps a corner more
 * than 1023 pixels off the screen's middle into a spike, and SGL puts a corner behind the camera
 * on the screen anyway. So each rectangle is first cut to the part of its plane the camera can
 * see (surround_clip): the four points where the screen's corners, a little wider, meet that
 * plane (view_floor_quad, view.h). What's left is a convex polygon of up to 8 corners, every one
 * of them on or just off the screen.
 */

#define SURROUND_RIM 80		// the rim's width: a pixel or two from high up, like the walls' top edge
#define SURROUND_EXTENT WORLD_COORD_LIMIT // out to where world coordinates stop
#define SURROUND_PIECES 8	// 4 sides x (rim, body)
#define SURROUND_CLIP_MAX 8 // corners of a rectangle cut to a quad: 4, plus one for each side cut

// Which wall a piece lies behind: the same order as level.c's sides
enum
{
	SURROUND_NORTH,
	SURROUND_EAST,
	SURROUND_SOUTH,
	SURROUND_WEST
};

typedef struct
{
	world_rect area;
	unsigned char side; // SURROUND_NORTH ...
	bool rim;			// the glowing rim along the wall's top, or the dark ground beyond
} surround_piece;

// The ring's pieces round an arena `half` from its middle to each wall. Writes SURROUND_PIECES.
void surround_pieces(int half, surround_piece *out);

/*
 * Cuts a rectangle to a convex quad (its corners in order round it, either way): Sutherland-
 * Hodgman, the quad cut by each of the rectangle's four sides in turn. Writes the corners of
 * what's left, in order round it, and returns how many (0: they don't meet; else 3 to 8).
 * Each side that cuts across an edge of the quad costs one division: a handful a frame, not
 * one per vertex or pixel.
 */
int surround_clip(const world_rect *r, const world_point quad[4], world_point *out);

/*
 * Whether a piece is between the camera and the arena: the camera is out past that piece's wall
 * (the stands can be outside the arena). Then it's drawn in front of everything (what's behind
 * it is under the raised ground); otherwise behind everything (seen from inside, over the walls'
 * tops, it hides nothing). eye: the camera's spot on the floor.
 */
bool surround_in_front(int side, world_point eye, int half);

#endif
