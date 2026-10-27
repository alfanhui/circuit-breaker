#include "static_mesh.h"

void static_mesh_init(static_mesh *m, POINT *points, unsigned short max_points,
					  POLYGON *polygons, ATTR *attributes, unsigned short max_polygons)
{
	m->mesh.data.pntbl = points;
	m->mesh.data.nbPoint = 0;
	m->mesh.data.pltbl = polygons;
	m->mesh.data.nbPolygon = 0;
	m->mesh.data.attbl = attributes;
	m->max_points = max_points;
	m->max_polygons = max_polygons;
}

// Load time only, so a linear search is fine
static int find_or_add_point(static_mesh *m, const jo_pos3D *p)
{
	POINT *points = m->mesh.data.pntbl;
	FIXED x = jo_int2fixed(p->x);
	FIXED y = jo_int2fixed(p->y);
	FIXED z = jo_int2fixed(p->z);
	unsigned int i;

	for (i = 0; i < m->mesh.data.nbPoint; ++i)
		if (points[i][X] == x && points[i][Y] == y && points[i][Z] == z)
			return i;
	if (i >= m->max_points)
		return -1;
	points[i][X] = x;
	points[i][Y] = y;
	points[i][Z] = z;
	m->mesh.data.nbPoint++;
	return i;
}

int static_mesh_add_quad(static_mesh *m, const jo_pos3D corners[4])
{
	int index = m->mesh.data.nbPolygon;
	POLYGON *polygon = m->mesh.data.pltbl + index;
	ATTR *attribute = m->mesh.data.attbl + index;

	if (index >= m->max_polygons)
		return -1;
	for (int i = 0; i < 4; ++i)
	{
		int point = find_or_add_point(m, &corners[i]);
		if (point < 0)
			return -1;
		polygon->Vertices[i] = point;
	}
	polygon->norm[X] = JO_FIXED_0;
	polygon->norm[Y] = JO_FIXED_1;
	polygon->norm[Z] = JO_FIXED_0;

	// Same defaults as jo_3d_create_plane, so jo's texture/colour setters behave the same
	JO_ZERO(attribute->sort);
	JO_ZERO(attribute->atrb);
	JO_ZERO(attribute->gstb);
	attribute->flag = Dual_Plane;

	m->mesh.data.nbPolygon++;
	return index;
}
