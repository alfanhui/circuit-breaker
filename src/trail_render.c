#include "trail_render.h"
#include "object_fog_vdp1.h"

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
		// Fog: VDP1 shades each corner by its own Gouraud colour (object_fog_vdp1.h)
		if (object_fog_enabled)
			m->attributes[i].atrb |= CL_Gouraud;
	}
	m->mesh.data.nbPoint = 0;
	m->mesh.data.nbPolygon = 0;
}

void trail_mesh_begin(trail_mesh *m)
{
	m->columns_used = 0;
	m->pieces_used = 0;
}

void trail_mesh_add(trail_mesh *m, const trail *t, int height, int head_gap, world_point focus,
					const world_rect *view, const wall_clip_camera *camera, const trail_occluders *occluders)
{
	// This trail's columns go after any added already this frame. Its first column is never
	// joined (it starts the trail), so no piece bridges from the last trail to this one
	const int first = m->columns_used;
	trail_column *mine = &m->columns[first];
	int columns =
		trail_build_columns(t, head_gap, focus, view, occluders, camera, mine, TRAIL_DRAW_COLUMNS_MAX - first);
	int pieces = m->pieces_used;
	unsigned char fog_levels[TRAIL_DRAW_COLUMNS_MAX]; // per column: its top and bottom are equally far

	if (camera != JO_NULL)
		columns = trail_clip_columns(mine, columns, camera, TRAIL_MIN_FORWARD, mine);

	for (int i = first; i < first + columns; ++i)
	{
		FIXED x = jo_int2fixed(m->columns[i].p.x);
		FIXED y = jo_int2fixed(m->columns[i].p.y);

		m->points[i * 2][X] = x;
		m->points[i * 2][Y] = y;
		m->points[i * 2][Z] = JO_FIXED_0;
		m->points[i * 2 + 1][X] = x;
		m->points[i * 2 + 1][Y] = y;
		m->points[i * 2 + 1][Z] = jo_int2fixed(height);
		if (object_fog_enabled)
			fog_levels[i] = (unsigned char)object_fog_level(m->columns[i].p.x, m->columns[i].p.y);

		// A joined column closes a piece with the column before it
		if (m->columns[i].joined && i > first)
		{
			if (object_fog_enabled) // Gouraud corners in the same order as the vertices below
				object_fog_set_quad(&m->attributes[pieces], fog_levels[i - 1], fog_levels[i - 1], fog_levels[i],
									fog_levels[i]);
			POLYGON *piece = &m->polygons[pieces++];
			piece->Vertices[0] = (i - 1) * 2;	  // from, bottom
			piece->Vertices[1] = (i - 1) * 2 + 1; // from, top
			piece->Vertices[2] = i * 2 + 1;		  // to, top
			piece->Vertices[3] = i * 2;			  // to, bottom
		}
	}
	m->columns_used = first + columns;
	m->pieces_used = pieces;
}

void trail_mesh_end(trail_mesh *m)
{
	if (m->pieces_used == 0)
		return;
	m->mesh.data.nbPoint = m->columns_used * 2;
	m->mesh.data.nbPolygon = m->pieces_used;
	jo_3d_mesh_draw(&m->mesh);
}

void trail_mesh_draw(trail_mesh *m, const trail *t, int height, int head_gap, world_point focus,
					 const world_rect *view, const wall_clip_camera *camera, const trail_occluders *occluders)
{
	trail_mesh_begin(m);
	trail_mesh_add(m, t, height, head_gap, focus, view, camera, occluders);
	trail_mesh_end(m);
}
