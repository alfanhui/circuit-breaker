#ifndef __TRAIL_RENDER_H__
#define __TRAIL_RENDER_H__

#include <jo/jo.h>
#include "game/trail.h"
#include "game/trail_pieces.h"

/*
 * Draws a trail as one SGL polygon list of flat-coloured, double-sided quads in world space,
 * with a single slPutPolygon. The trail is cut into short pieces near the player each frame
 * (see game/trail_pieces.h); pieces in a strip share their edge vertices.
 */

#define TRAIL_DRAW_PIECES_MAX (TRAIL_DRAW_COLUMNS_MAX - 1)

typedef struct
{
	jo_3d_mesh mesh;
	trail_column columns[TRAIL_DRAW_COLUMNS_MAX];
	POINT points[TRAIL_DRAW_COLUMNS_MAX * 2]; // bottom and top of every column
	POLYGON polygons[TRAIL_DRAW_PIECES_MAX];
	ATTR attributes[TRAIL_DRAW_PIECES_MAX];
} trail_mesh;

void trail_mesh_init(trail_mesh *m, jo_color color);

// Call inside view_push_world_matrix(). head_gap keeps the wall starting behind the cycle;
// only what's inside view is drawn, cut finer near focus (the player).
// camera: the 1st person camera, to keep corners in front of it (trail_clip_columns); NULL otherwise.
void trail_mesh_draw(trail_mesh *m, const trail *t, int head_gap, world_point focus, const world_rect *view,
					 const wall_clip_camera *camera);

#endif
