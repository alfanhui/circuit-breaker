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
	jo_pos3D_fixed points[TRAIL_DRAW_COLUMNS_MAX * 2]; // bottom and top of every column
	jo_3d_polygon polygons[TRAIL_DRAW_PIECES_MAX];
	jo_3d_polygon_attribute attributes[TRAIL_DRAW_PIECES_MAX];
	int columns_used; // between trail_mesh_begin and _end: what's filled so far this frame
	int pieces_used;
} trail_mesh;

void trail_mesh_init(trail_mesh *m, jo_color color);

// Call inside view_push_world_matrix(). head_gap keeps the wall starting behind the cycle;
// only what's inside view is drawn, cut finer near focus (the player).
// camera: the 1st person camera, to keep corners in front of it (trail_clip_columns); NULL otherwise.
// occluders: nearer trails that hide parts of this one (game/trail_occlusion.h); NULL: none.
// height: how tall it stands (TRAIL_HEIGHT; less while a dead rider's trail sinks, rider.h)
void trail_mesh_draw(trail_mesh *m, const trail *t, int height, int head_gap, world_point focus,
					 const world_rect *view, const wall_clip_camera *camera, const trail_occluders *occluders);

// Several trails as one mesh, drawn with a single slPutPolygon: begin, add each (the same
// arguments as trail_mesh_draw), then end. A mesh mustn't be filled again for another draw in
// the same frame: SGL can still be reading it (the prologue's message drew each of its strokes
// through one mesh, and some came out as spikes and stray diagonals). Up to
// TRAIL_DRAW_COLUMNS_MAX columns between them; past that, the rest aren't drawn
void trail_mesh_begin(trail_mesh *m);
void trail_mesh_add(trail_mesh *m, const trail *t, int height, int head_gap, world_point focus,
					const world_rect *view, const wall_clip_camera *camera, const trail_occluders *occluders);
void trail_mesh_end(trail_mesh *m);

#endif
