#ifndef __TRAIL_RENDER_H__
#define __TRAIL_RENDER_H__

#include <jo/jo.h>
#include "game/trail.h"

/*
 * Draws a trail as one SGL polygon list: one flat-coloured, double-sided quad per segment,
 * with vertices in world space. Rebuilt every frame, so it is drawn with a single slPutPolygon.
 */

typedef struct
{
	jo_3d_mesh mesh;
	POINT points[TRAIL_MAX_SEGMENTS * 4];
	POLYGON polygons[TRAIL_MAX_SEGMENTS];
	ATTR attributes[TRAIL_MAX_SEGMENTS];
} trail_mesh;

void trail_mesh_init(trail_mesh *m, jo_color color);

// Call inside view_push_world_matrix(). head_gap keeps the wall starting behind the cycle.
void trail_mesh_draw(trail_mesh *m, const trail *t, int head_gap);

#endif
