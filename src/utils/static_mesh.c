#include "static_mesh.h"

void static_mesh_init(static_mesh *m, jo_pos3D_fixed *points, unsigned short max_points,
					  jo_3d_polygon *polygons, jo_3d_polygon_attribute *attributes, unsigned short max_polygons)
{
	m->mesh.data.points = points;
	m->mesh.data.point_count = 0;
	m->mesh.data.polygons = polygons;
	m->mesh.data.polygon_count = 0;
	m->mesh.data.attributes = attributes;
	m->max_points = max_points;
	m->max_polygons = max_polygons;
}

// Load time only, so a linear search is fine
static int find_or_add_point(static_mesh *m, const jo_pos3D *p)
{
	jo_pos3D_fixed *points = m->mesh.data.points;
	jo_fixed x = jo_int2fixed(p->x);
	jo_fixed y = jo_int2fixed(p->y);
	jo_fixed z = jo_int2fixed(p->z);
	unsigned int i;

	for (i = 0; i < m->mesh.data.point_count; ++i)
		if (points[i].x == x && points[i].y == y && points[i].z == z)
			return i;
	if (i >= m->max_points)
		return -1;
	points[i].x = x;
	points[i].y = y;
	points[i].z = z;
	m->mesh.data.point_count++;
	return i;
}

int static_mesh_add_quad(static_mesh *m, const jo_pos3D corners[4])
{
	int index = m->mesh.data.polygon_count;
	jo_3d_polygon *polygon = m->mesh.data.polygons + index;
	jo_3d_polygon_attribute *attribute = m->mesh.data.attributes + index;

	if (index >= m->max_polygons)
		return -1;
	for (int i = 0; i < 4; ++i)
	{
		int point = find_or_add_point(m, &corners[i]);
		if (point < 0)
			return -1;
		polygon->vertices[i] = point;
	}
	polygon->normal.x = JO_FIXED_0;
	polygon->normal.y = JO_FIXED_1;
	polygon->normal.z = JO_FIXED_0;

	// Same defaults as jo_3d_create_plane, so jo's texture/colour setters behave the same
	JO_ZERO(attribute->sort);
	JO_ZERO(attribute->draw_mode);
	JO_ZERO(attribute->gouraud_table);
	attribute->plane = JO_3D_DUAL_PLANE;

	m->mesh.data.polygon_count++;
	return index;
}
