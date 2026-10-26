#include "level.h"
#include "utils/static_mesh.h"

#define WALL_HALF_HEIGHT 128
#define WALL_CENTRE_Z 120 // walls stand on the floor (z = 0), sinking 8 units below it to hide the seam

#define WALL_QUADS (4 * WORLD_WALL_SEGMENTS)
#define WALL_POINTS (WALL_QUADS * 2) // neighbouring segments share their edges

// All four sides are one polygon list in world space: a single slPutPolygon, no per-segment matrices
static POINT wall_points[WALL_POINTS];
static POLYGON wall_polygons[WALL_QUADS];
static ATTR wall_attributes[WALL_QUADS];
static static_mesh walls;

static int box1_texture_id = 0;
static int box2_texture_id = 0;
static int box3_texture_id = 0;
static int box4_texture_id = 0;

void load_arena_textures(void)
{
	box1_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX1.TGA", JO_COLOR_Transparent);
	box2_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX2.TGA", JO_COLOR_Transparent);
	box3_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX3.TGA", JO_COLOR_Transparent);
	box4_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX4.TGA", JO_COLOR_Transparent);
}

// shape is one of jo's plane vertex macros, whose corner order sets the texture orientation.
// fixed is the coordinate shared by the whole side, step runs along it (+1 or -1).
static void add_wall_side(jo_vertice shape[4], int texture_id, bool along_x, int fixed, int step)
{
	int first = -step * (WORLD_ARENA_HALF - WORLD_WALL_SEGMENT_HALF);
	jo_pos3D corners[4];

	for (int i = 0; i < WORLD_WALL_SEGMENTS; ++i)
	{
		int along = first + step * i * 2 * WORLD_WALL_SEGMENT_HALF;
		int centre_x = along_x ? along : fixed;
		int centre_y = along_x ? fixed : along;

		for (int c = 0; c < 4; ++c)
		{
			corners[c].x = centre_x + shape[c].x;
			corners[c].y = centre_y + shape[c].y;
			corners[c].z = WALL_CENTRE_Z + shape[c].z;
		}
		int polygon = static_mesh_add_quad(&walls, corners);
		if (polygon >= 0)
			jo_3d_set_mesh_polygon_texture(&walls.mesh, texture_id, polygon);
	}
}

void init_arena_walls(void)
{
	// World north is -y and east is -x (see world.h)
	jo_vertice south[4] = JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(WORLD_WALL_SEGMENT_HALF, WALL_HALF_HEIGHT);
	jo_vertice east[4] = JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(WORLD_WALL_SEGMENT_HALF, WALL_HALF_HEIGHT);
	jo_vertice north[4] = JO_3D_PLANE_RECTANGLE_VERTICES_X(WORLD_WALL_SEGMENT_HALF, WALL_HALF_HEIGHT);
	jo_vertice west[4] = JO_3D_PLANE_RECTANGLE_VERTICES_Y(WORLD_WALL_SEGMENT_HALF, WALL_HALF_HEIGHT);

	static_mesh_init(&walls, wall_points, WALL_POINTS, wall_polygons, wall_attributes, WALL_QUADS);
	add_wall_side(south, box1_texture_id, true, WORLD_ARENA_HALF, -1);
	add_wall_side(east, box2_texture_id, false, -WORLD_ARENA_HALF, 1);
	add_wall_side(north, box3_texture_id, true, -WORLD_ARENA_HALF, 1);
	add_wall_side(west, box4_texture_id, false, WORLD_ARENA_HALF, -1);
}

void draw_arena_walls(void)
{
	jo_3d_mesh_draw(&walls.mesh);
}
