#ifndef __TRAIL_RENDER_H__
#define __TRAIL_RENDER_H__

#include <jo/jo.h>
#include "game/trail.h"

/*
 * Draws a trail as one SGL polygon list: one flat-coloured, double-sided quad per segment,
 * with vertices in world space, drawn with a single slPutPolygon.
 * Neighbouring segments share their corner vertices. Corner vertices are only rewritten when
 * the trail's corners change; each frame only the two live-end vertices move.
 */

#define TRAIL_MESH_POINTS ((TRAIL_MAX_CORNERS + 1) * 2) // bottom and top of every corner, plus the head

typedef struct
{
	jo_3d_mesh mesh;
	POINT points[TRAIL_MESH_POINTS];
	POLYGON polygons[TRAIL_MAX_SEGMENTS];
	ATTR attributes[TRAIL_MAX_SEGMENTS];
	unsigned short built_version;
	bool built;
} trail_mesh;

void trail_mesh_init(trail_mesh *m, jo_color color);

// Call inside view_push_world_matrix(). head_gap keeps the wall starting behind the cycle.
void trail_mesh_draw(trail_mesh *m, const trail *t, int head_gap);

#endif
