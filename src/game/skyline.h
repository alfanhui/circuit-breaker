#ifndef __SKYLINE_H__
#define __SKYLINE_H__

#include <stdbool.h>

/*
 * Towers that stand behind nearer, taller towers aren't drawn (level.c, the city: game/city.h).
 *
 * VDP1 has no depth buffer: everything sent is filled, the hidden too. Up on the entry's roof the
 * city's below you and you see roofs, each a few pixels of side. Down at the CD block's hub, 1400
 * lower, every tower stands over you: each one's sides fill a column of the screen from top to
 * bottom, dozens deep, one over another. VDP1 couldn't finish a frame (30 fps, and the roof at
 * your feet, drawn last, left half filled: play-test 2026-11-12).
 *
 * Towers are boxes standing on the same grid, so what hides what can be worked out on the floor
 * plan alone. From the eye, a box's outline on the plan is the wedge between the two corners
 * furthest apart as you see them (its silhouette). Box B can't be seen if
 *   - every corner of B is inside hider A's wedge, and beyond the line through A's two silhouette
 *     corners (so behind it, not in front or beside), and
 *   - A's top is no lower than B's, and no lower than the eye: then any line of sight to any part
 *     of B, which ends no higher than A's top and starts no higher, is under A's top where it
 *     passes A, so it hits A first.
 * Only a few hiders a frame: the near, tall boxes in front of you (skyline_offer keeps the
 * SKYLINE_HIDERS best: nearest, less how far each stands over the eye). Testing a box against one is at most 12 cross products (two multiplies
 * each), stopping at the first corner that's outside: no division, no square root.
 *
 * Exact for what it hides (never something in view), but it misses what two hiders hide only
 * between them. Pure C: unit-tested on the PC (tests/test_skyline.c)
 */

#define SKYLINE_HIDERS 8

// A box on the floor plan, and its top
typedef struct
{
	short x_min, x_max, y_min, y_max;
	short top;
} skyline_box;

typedef struct
{
	int ax, ay, bx, by; // its silhouette's two corners from the eye, b to the left of a (a x b > 0)
	int reach;			// how far it is (its nearest point, the longer way along x or y), less its height over the eye
	int far;			// no less than how far its furthest corner is (for how steeply its top's seen)
	short top;
} skyline_hider;

typedef struct
{
	int x, y, z;	// the eye
	int fx, fy;		// the way it looks along the floor (any length)
	int count;
	int worst;		 // which of the kept hiders is the worst (once they're all taken)
	skyline_hider hiders[SKYLINE_HIDERS];
} skyline;

// A new frame: the eye, and the way it looks along the floor (hiders behind you aren't kept)
void skyline_begin(skyline *s, int x, int y, int z, int forward_x, int forward_y);

// Offers a box as a hider. Kept if its top's above the eye, it's in front, the eye's not over it,
// and it's among the best so far (near and tall). Over a hundred are offered a frame and eight
// kept, so the tests that turn most away come first and cost a few compares: its silhouette (and
// the in-front test's multiplies) are only worked out for one that will be kept. (Worked out for
// every offer, picking the hiders was 9-11% of a frame, whichever way you looked: 2026-11-13)
void skyline_offer(skyline *s, const skyline_box *b);

/*
 * Whether the box is wholly behind one of the hiders: in its shadow on the plan, and under its
 * top as the eye sees them. Two ways the second can hold:
 *   - level: the hider's top is no lower than the box's (and both the hider's and the eye's
 *     rule above). True wherever in the shadow the box is, and whatever the eye's height.
 *   - steeper: the hider's top is seen at a steeper angle than any of the box. A near tower
 *     hides a far one that's taller than it, as a hand hides a mountain. The hider's top rises
 *     (its top - eye) over at most `far`; the box's rises (its top - eye) over at least its
 *     nearest point's distance: compared by cross-multiplying, no division. This is what hides
 *     the far skyline (level.c) behind the towers round you: it's taller than all of them
 * skyline_hides_level: only the first way. The table of what's seen from each roof (city.h) goes
 * by that one, as it has to hold for every eye height on the roof
 */
bool skyline_hides(const skyline *s, const skyline_box *b);
bool skyline_hides_level(const skyline *s, const skyline_box *b);

/*
 * A tower that's only partly behind a hider is still drawn whole by the test above, and VDP1
 * fills the hidden part of each of its sides all the same. Looking along a row of towers each
 * one shows a sliver past the one in front, and the whole of every side was filled, one over
 * another: VDP1 couldn't finish the frame, and the roof under you, drawn last, was left cut off
 * in a wedge ("the floors are clipping on the edges": the user, 2026-11-13).
 * So a side is cut back to the part that shows. A side on the plan is a line along x or y: the
 * line y = `line` from x = *from to x = *to if along_x, else x = `line` from y = *from to *to
 * (*from under *to), `top` high. For each hider no lower than it (the level rule, above):
 *   - both ends in its shadow: all of it is (a shadow's a wedge cut by a line: no dents), false
 *   - one end in it: that end's moved along to where the side comes out of the shadow. Found by
 *     halving, SKYLINE_TRIM_STEPS times, with the same test the ends had: no division. It stops
 *     short, inside the shadow: a sliver of hidden side is still drawn, never a gap
 *   - neither: left alone (a hider across its middle only: two pieces, not worth it)
 * Returns false if nothing of the side's left to draw
 */
#define SKYLINE_TRIM_STEPS 6 // to within a 64th of the side's length
bool skyline_trim(const skyline *s, bool along_x, int line, int *from, int *to, int top);

#endif
