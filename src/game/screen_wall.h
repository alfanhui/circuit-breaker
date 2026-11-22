#ifndef __SCREEN_WALL_H__
#define __SCREEN_WALL_H__

#include <stdbool.h>
#include "frustum.h"

/*
 * SG-9 (docs/BACKLOG.md): a wall that fills the whole screen hides everything behind it.
 *
 * Close to a tall wall (a guard page's side, beside you in the city), VDP1 still filled all the
 * towers behind it: it has no depth buffer, and spends its time on every pixel it's given, hidden
 * or not. jo can leave out every polygon that's wholly further than a depth (jo_3d_set_depth_limit:
 * jo's cells use it for a room's wall, "up to 50 ms against a wall"). So once a frame: is there a
 * wall in front of you that covers the whole screen? Then nothing further than its furthest point
 * on the screen can be seen, and the limit is that depth.
 *
 * The test: the rays through the screen's four corners (a little wider, as frustum_slopes does).
 * An upright rectangle (on the plan a line along x or y; from its bottom to its top) covers the
 * screen if all four rays hit it in front of the eye. It's flat, so the part of it on the screen
 * is the quad between those four hits: none of it's further than the furthest of them, and
 * anything wholly further than that is behind it, on whichever ray it's seen.
 *
 * No division for the hits (cross-multiplied compares); one for each hit's depth, four a wall, and
 * only for the few walls near you (level.c): once a frame. Pure C: tests/test_screen_wall.c
 */

/*
 * Every corner's hit must be at least this deep (world units), or the wall isn't taken. Nearer, it's
 * cut by the near plane (view.c's VIEW_NEAR 8, and jo's own): not drawn there, and what's behind
 * shows through. The first film of this (wall_turn.steps, 2026-11-18) had the eye 1 from a wall's
 * plane: the wall wasn't drawn, the limit left out everything else, and the screen showed only the
 * sky and the board. Four times the near plane, and still under WALKER_RADIUS (40): a wall you're
 * stood against, straight on, is taken
 */
#define SCREEN_WALL_NEAREST 32

/*
 * The deepest of the four corners' hits on the rectangle, along the view (world units, as the
 * frustum's: the depth from the eye along forward), or -1 if it doesn't cover the screen (or a hit's
 * nearer than SCREEN_WALL_NEAREST).
 * along_x: the rectangle's in the plane y = line, from x = from to x = to; else x = line, y from
 * `from` to `to`. z from bottom to top
 */
int screen_wall_depth(const frustum *f, bool along_x, int line, int from, int to, int bottom, int top);

#endif
