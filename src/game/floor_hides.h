#ifndef __FLOOR_HIDES_H__
#define __FLOOR_HIDES_H__

/*
 * What the floor you stand on hides: a way of leaving out work that holds for any level built of
 * boxes, not only the city (docs/LEVELS.md, "Building a level that VDP1 can draw").
 *
 * VDP1 has no depth buffer: it draws every polygon it's given, far first, and the roof under your
 * feet, drawn last, paints over whatever was behind it. In the city that was most of VDP1's work:
 * the towers' sides run from far above to 3000 below, and the half of the screen your roof covers
 * was filled with them first (2026-11-15, tests/e2e/roof_cut_run.steps: ~7 screens of pixels a
 * frame, and the roof underfoot cut off when VDP1 ran out of time).
 *
 * The idea. Stand with your eye at E, over a floor: a rectangle at height floor_z. A point P below
 * the floor is hidden by it when the line from E to P passes through the rectangle. All such points
 * make one shape: a cone from E through the rectangle, below it (a "shadow" of the floor, as if a
 * light were at your eye). Two things follow:
 *
 *   - For an upright column at (x, y), the cone holds everything below one height: the column is
 *     hidden below floor_hides_below(x, y). An upright face (a wall, a tower's side) needn't be
 *     drawn below that height at either end. Joining the two ends' heights with a straight line is
 *     safe too: along a wall the hidden height curves down away from you (it's concave), so the
 *     straight line between its ends is under it everywhere: it never cuts off anything that shows.
 *     But only the hidden heights themselves: not after raising them to the face's own foot at
 *     an end (floor_hides_line).
 *   - The cone is convex, so a shape whose corners are all in it is all in it: a box whose four
 *     top corners are hidden (floor_hides_box) isn't drawn at all, sides, top and all.
 *
 * Working it out. P is seen through the floor's edge at a fraction t of the way from E to P along
 * the floor: t = (distance from E to that edge, that way) / (distance from E to P, that way). P is
 * hidden when the line has dropped below floor_z by then: E.z - P.z >= (E.z - floor_z) / t. So the
 * hidden height is E.z - H * ratio, H the eye's height over the floor, ratio = 1 / t, the larger of
 * the x and y ways (the edge reached first). The four reciprocals (1 / the distance to each edge)
 * are worked out once, when you stand somewhere (floor_hides_begin: four divisions a frame); every
 * point after that is multiplies and shifts. Points outside the floor's own column only.
 *
 * Always errs towards drawing: when the eye isn't over the floor, or is right at an edge, or a
 * point's too far to work out in 32 bits, nothing's hidden. Pure C, no jo: tests/test_floor_hides.c.
 */

// Nothing's hidden at this height or above (any height in the game is above it)
#define FLOOR_HIDES_NOTHING (-32000)

typedef struct
{
	int on; // 1: the eye's over the floor, above it; 0: hides nothing
	int eye_x, eye_y, eye_z, floor_z;
	// 1 / the distance from the eye to each edge, x 2^20 (0: too close to that edge to say)
	int inv_x_min, inv_x_max, inv_y_min, inv_y_max;
} floor_hides;

// Stood with the eye at (eye_x, eye_y, eye_z) over the floor x_min..x_max, y_min..y_max at floor_z.
// Off it, or not above it: hides nothing. Once a frame (four divisions)
void floor_hides_begin(floor_hides *f, int eye_x, int eye_y, int eye_z, int x_min, int x_max, int y_min, int y_max,
					   int floor_z);

// Nothing hidden (off a floor, falling, flying)
void floor_hides_none(floor_hides *f);

// Below what height the column at (x, y) is hidden by the floor (FLOOR_HIDES_NOTHING: none of it)
int floor_hides_below(const floor_hides *f, int x, int y);

// Whether a box x_min..x_max, y_min..y_max up to top is hidden by the floor, all of it
int floor_hides_box(const floor_hides *f, int x_min, int x_max, int y_min, int y_max, int top);

/*
 * An upright face's foot, as a straight line: its height at its two ends (*from, *to). It starts as
 * the face's own foot, `foot` all along (where it stands on something), and each line offered that
 * leaves out more of it (floor_hides_below at the two ends, or the screen's bottom edge's) takes its
 * place, if it can.
 *
 * Why not simply the higher of the two at each end, then a straight line between: the visible part
 * of the face starts at the higher of a constant (its foot) and a curve (where it comes out from
 * behind the floor) at every point along it, and that's a shape with a corner where they cross. A
 * straight line between the two ends cuts across the corner, above it: it cut a wedge out of a wall
 * beside a lower roof (the user, 2026-11-15, at 0650: the street's depths showing between the wall's
 * foot and the roof). So a line is taken only if it's at or above the foot at both ends (then it's
 * above the foot all along). It's under the curve all along too, as the curve bulges upwards
 * between its ends (concave): so it's under the visible part, and cuts off nothing that shows.
 * Of two lines that may stand, the one higher on the whole (the larger sum of its ends). The face's
 * top is the same upside down: offer it the lines negated (floor_hides_line(-top, ...)).
 */
void floor_hides_line(int foot, int *from, int *to, int line_from, int line_to);

#endif
