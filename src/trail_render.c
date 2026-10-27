#include "trail_render.h"

#define TRAIL_HEIGHT 60

void trail_mesh_init(trail_mesh *m, jo_color color)
{
	m->mesh.data.pntbl = m->points;
	m->mesh.data.pltbl = m->polygons;
	m->mesh.data.nbPolygon = TRAIL_MAX_SEGMENTS; // for jo's index check while colouring
	m->mesh.data.attbl = m->attributes;

	// Segment i runs from corner i to corner i + 1 (the head counts as the corner after the last)
	for (int i = 0; i < TRAIL_MAX_SEGMENTS; ++i)
	{
		m->polygons[i].norm[X] = JO_FIXED_0;
		m->polygons[i].norm[Y] = JO_FIXED_1;
		m->polygons[i].norm[Z] = JO_FIXED_0;
		m->polygons[i].Vertices[0] = i * 2;			  // from, bottom
		m->polygons[i].Vertices[1] = i * 2 + 1;		  // from, top
		m->polygons[i].Vertices[2] = (i + 1) * 2 + 1; // to, top
		m->polygons[i].Vertices[3] = (i + 1) * 2;	  // to, bottom

		JO_ZERO(m->attributes[i].sort);
		JO_ZERO(m->attributes[i].atrb);
		JO_ZERO(m->attributes[i].gstb);
		m->attributes[i].flag = Dual_Plane;
		jo_3d_set_mesh_polygon_color(&m->mesh, color, i);
	}
	m->mesh.data.nbPoint = 0;
	m->mesh.data.nbPolygon = 0;
	m->built = false;
}

static void set_column(trail_mesh *m, int index, world_point p)
{
	FIXED x = jo_int2fixed(p.x);
	FIXED y = jo_int2fixed(p.y);

	m->points[index * 2][X] = x;
	m->points[index * 2][Y] = y;
	m->points[index * 2][Z] = JO_FIXED_0;
	m->points[index * 2 + 1][X] = x;
	m->points[index * 2 + 1][Y] = y;
	m->points[index * 2 + 1][Z] = jo_int2fixed(TRAIL_HEIGHT);
}

void trail_mesh_draw(trail_mesh *m, const trail *t, int head_gap)
{
	world_point head;
	int corners = t->corner_count;
	int segments = corners > 0 ? corners - 1 : 0;

	if (!m->built || m->built_version != t->version)
	{
		for (int i = 0; i < corners; ++i)
			set_column(m, i, t->corners[i]);
		m->built_version = t->version;
		m->built = true;
	}

	if (trail_get_drawn_head(t, head_gap, &head))
	{
		set_column(m, corners, head);
		++segments;
		m->mesh.data.nbPoint = (corners + 1) * 2;
	}
	else
	{
		m->mesh.data.nbPoint = corners * 2;
	}

	if (segments == 0)
		return;
	m->mesh.data.nbPolygon = segments;
	jo_3d_mesh_draw(&m->mesh);
}
