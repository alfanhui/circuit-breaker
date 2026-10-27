#ifndef __STATIC_MESH_H__
#define __STATIC_MESH_H__

#include <jo/jo.h>

/*
 * Builds one SGL polygon list from quads at load time, so a whole object (all arena walls,
 * the cycle) is a single slPutPolygon with no per-quad matrix work. Corners shared between
 * quads are stored once, so SGL transforms each vertex only once per frame.
 * Storage is caller provided: no malloc.
 */

typedef struct
{
	jo_3d_mesh mesh;
	unsigned short max_points;
	unsigned short max_polygons;
} static_mesh;

void static_mesh_init(static_mesh *m, POINT *points, unsigned short max_points,
					  POLYGON *polygons, ATTR *attributes, unsigned short max_polygons);

// Corners are in the mesh's own space, in the same order as jo's plane vertex macros
// (the order sets the texture orientation). Returns the polygon index, or -1 when full.
int static_mesh_add_quad(static_mesh *m, const jo_pos3D corners[4]);

#endif
