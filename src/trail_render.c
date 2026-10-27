#include "trail_render.h"

#define TRAIL_HEIGHT 60

void trail_mesh_init(trail_mesh *m, jo_color color)
{
	m->mesh.data.pntbl = m->points;
	m->mesh.data.pltbl = m->polygons;
	m->mesh.data.nbPolygon = TRAIL_DRAW_PIECES_MAX; // for jo's index check while colouring
	m->mesh.data.attbl = m->attributes;

	// Every piece looks the same, so attributes are set once; vertex indices change per frame
	for (int i = 0; i < TRAIL_DRAW_PIECES_MAX; ++i)
	{
		m->polygons[i].norm[X] = JO_FIXED_0;
		m->polygons[i].norm[Y] = JO_FIXED_1;
		m->polygons[i].norm[Z] = JO_FIXED_0;

		JO_ZERO(m->attributes[i].sort);
		JO_ZERO(m->attributes[i].atrb);
		JO_ZERO(m->attributes[i].gstb);
		m->attributes[i].flag = Dual_Plane;
		jo_3d_set_mesh_polygon_color(&m->mesh, color, i);
	}
	m->mesh.data.nbPoint = 0;
	m->mesh.data.nbPolygon = 0;
}

void trail_mesh_draw(trail_mesh *m, const trail *t, int head_gap, world_point focus, const world_rect *view,
					 const wall_clip_camera *camera)
{
	int columns = trail_build_columns(t, head_gap, focus, view, m->columns, TRAIL_DRAW_COLUMNS_MAX);
	int pieces = 0;

	if (camera != JO_NULL)
		columns = trail_clip_columns(m->columns, columns, camera, TRAIL_MIN_FORWARD, m->columns);

	for (int i = 0; i < columns; ++i)
	{
		FIXED x = jo_int2fixed(m->columns[i].p.x);
		FIXED y = jo_int2fixed(m->columns[i].p.y);

		m->points[i * 2][X] = x;
		m->points[i * 2][Y] = y;
		m->points[i * 2][Z] = JO_FIXED_0;
		m->points[i * 2 + 1][X] = x;
		m->points[i * 2 + 1][Y] = y;
		m->points[i * 2 + 1][Z] = jo_int2fixed(TRAIL_HEIGHT);

		// A joined column closes a piece with the column before it
		if (m->columns[i].joined)
		{
			POLYGON *piece = &m->polygons[pieces++];
			piece->Vertices[0] = (i - 1) * 2;	  // from, bottom
			piece->Vertices[1] = (i - 1) * 2 + 1; // from, top
			piece->Vertices[2] = i * 2 + 1;		  // to, top
			piece->Vertices[3] = i * 2;			  // to, bottom
		}
	}

	if (pieces == 0)
		return;
	m->mesh.data.nbPoint = columns * 2;
	m->mesh.data.nbPolygon = pieces;
	jo_3d_mesh_draw(&m->mesh);
}
