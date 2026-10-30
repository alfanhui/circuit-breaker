#ifndef __TRAIL_PIECES_H__
#define __TRAIL_PIECES_H__

#include "trail.h"
#include "trail_occlusion.h"
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
#define TRAIL_NEAR_PIECE 256 // a power of two: pieces are lined up on the player with a mask (trail_pieces.c)
#define TRAIL_FAR_PIECE 1000
#define TRAIL_DRAW_COLUMNS_MAX 200

typedef struct
{
	world_point p;
	unsigned char joined;
} trail_column;

// Keeps the parts of the trail inside view; focus (the player) sets how finely they're cut.
// occluders (NULL: none): leaves out the parts hidden behind a nearer trail (trail_occlusion.h).
// Returns the number of columns written to out (at most max_columns).
int trail_build_columns(const trail *t, int head_gap, world_point focus, const world_rect *view,
						const trail_occluders *occluders, trail_column *out, int max_columns);

/*
 * 1st person: keeps every strip's corners in front of the camera, like the walls (wall_clip.h).
 * A corner behind the camera projects to the wrong side of the screen and bends the piece.
 * Each strip is cut to its run of pieces that are in view and min_forward in front, with its
 * nearest end slid forward along the trail. Works in place (out may be columns).
 * Returns the number of columns left.
 */
// Near, but safely: the trail's base is 35 below the eye, and at 90 degrees a corner d in front
// projects 160 * 35 / d pixels down, inside VDP1's 1024 from ~6 away (SGL's own near limit,
// slZdspLevel 7, is ~5). Too far and the cut shows: the screen's edge shows what's as far to the
// side as it is in front (further while boosting), and collision lets a cycle run 25 beside a
// trail, so its cut-off near end showed as a hole there, the far side showing through
// (crash_slide.steps). At 10 the end is 2.5x further to the side than in front: well off-screen
#define TRAIL_MIN_FORWARD 10
int trail_clip_columns(const trail_column *columns, int count, const wall_clip_camera *camera,
					   int min_forward, trail_column *out);

#endif
