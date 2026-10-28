#ifndef __TRAIL_PIECES_H__
#define __TRAIL_PIECES_H__

#include "trail.h"
#include "wall_clip.h"

/*
 * Cuts a trail into short pieces for drawing, keeping only what the camera can see.
 * Pure C, unit-tested on the PC.
 *
 * Why: one quad per segment breaks down when segments get long.
 *   - VDP1 traces a polygon's whole outline even where it's off screen, so a 15000-long
 *     quad costs drawing time out of all proportion: frames drop (seen: 32 fps).
 *   - A long quad passing beside the camera has a corner that projects to a huge screen
 *     position, and the quad comes out skewed ("diagonal at weird angles").
 * Short pieces keep every corner reasonable; only what's inside the camera's view rectangle
 * (view_cull.h) is kept.
 * Pieces near the player are short; far ones can be longer, since they're small on screen.
 *
 * Output is a list of columns (points along the trail). A column with joined = 1 is
 * connected to the column before it by a piece; joined = 0 starts a new strip.
 * Segments are emitted newest first, so if max_columns runs out the oldest are dropped.
 */

#define TRAIL_NEAR_RANGE 2000 // inside this, pieces are short
#define TRAIL_NEAR_PIECE 250
#define TRAIL_FAR_PIECE 1000
#define TRAIL_DRAW_COLUMNS_MAX 200

typedef struct
{
	world_point p;
	unsigned char joined;
} trail_column;

// Keeps the parts of the trail inside view; focus (the player) sets how finely they're cut.
// Returns the number of columns written to out (at most max_columns).
int trail_build_columns(const trail *t, int head_gap, world_point focus, const world_rect *view,
						trail_column *out, int max_columns);

/*
 * 1st person: keeps every strip's corners in front of the camera, like the walls (wall_clip.h).
 * A corner behind the camera projects to the wrong side of the screen and bends the piece.
 * Each strip is cut to its run of pieces that are in view and min_forward in front, with its
 * nearest end slid forward along the trail. Works in place (out may be columns).
 * Returns the number of columns left.
 */
// The trail is 60 tall: its corners stay inside VDP1's range from ~6 away. A cycle closer than
// this to the side of a trail would see the cut on screen as a hole (turn_gate.h keeps U-turns 50 apart)
#define TRAIL_MIN_FORWARD 20
int trail_clip_columns(const trail_column *columns, int count, const wall_clip_camera *camera,
					   int min_forward, trail_column *out);

#endif
