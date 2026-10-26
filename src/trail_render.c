#include "trail_render.h"

#define TRAIL_HEIGHT 60

void trail_mesh_init(trail_mesh *m, jo_color color)
{
	m->mesh.data.pntbl = m->points;
	m->mesh.data.nbPoint = TRAIL_MAX_SEGMENTS * 4;
	m->mesh.data.pltbl = m->polygons;
	m->mesh.data.nbPolygon = TRAIL_MAX_SEGMENTS;
	m->mesh.data.attbl = m->attributes;

	for (int i = 0; i < TRAIL_MAX_SEGMENTS; ++i)
	{
		m->polygons[i].norm[X] = JO_FIXED_0;
		m->polygons[i].norm[Y] = JO_FIXED_1;
		m->polygons[i].norm[Z] = JO_FIXED_0;
		m->polygons[i].Vertices[0] = i * 4;
		m->polygons[i].Vertices[1] = i * 4 + 1;
		m->polygons[i].Vertices[2] = i * 4 + 2;
		m->polygons[i].Vertices[3] = i * 4 + 3;

		JO_ZERO(m->attributes[i].sort);
		JO_ZERO(m->attributes[i].atrb);
		JO_ZERO(m->attributes[i].gstb);
		m->attributes[i].flag = Dual_Plane;
		jo_3d_set_mesh_polygon_color(&m->mesh, color, i);
	}
	m->mesh.data.nbPoint = 0;
	m->mesh.data.nbPolygon = 0;
}

static void set_point(POINT p, int x, int y, int z)
{
	p[X] = jo_int2fixed(x);
	p[Y] = jo_int2fixed(y);
	p[Z] = jo_int2fixed(z);
}

void trail_mesh_draw(trail_mesh *m, const trail *t, int head_gap)
{
	world_point from;
	world_point to;
	int count = 0;
	int segments = trail_segment_count(t);

	for (int i = 0; i < segments; ++i)
	{
		if (!trail_get_segment(t, i, head_gap, &from, &to))
			continue;
		POINT *quad = m->points + count * 4;
		set_point(quad[0], from.x, from.y, 0);
		set_point(quad[1], from.x, from.y, TRAIL_HEIGHT);
		set_point(quad[2], to.x, to.y, TRAIL_HEIGHT);
		set_point(quad[3], to.x, to.y, 0);
		++count;
	}

	if (count == 0)
		return;
	m->mesh.data.nbPoint = count * 4;
	m->mesh.data.nbPolygon = count;
	jo_3d_mesh_draw(&m->mesh);
}
