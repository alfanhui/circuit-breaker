#include "level.h"

#define WALL_HALF_HEIGHT 128
#define WALL_CENTRE_Z 120 // walls stand on the floor (z = 0), sinking 8 units below it to hide the seam

#define WALL_VERTICES_X JO_3D_PLANE_RECTANGLE_VERTICES_X(WORLD_WALL_SEGMENT_HALF, WALL_HALF_HEIGHT)
#define WALL_VERTICES_FLIPPED_X JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(WORLD_WALL_SEGMENT_HALF, WALL_HALF_HEIGHT)
#define WALL_VERTICES_Y JO_3D_PLANE_RECTANGLE_VERTICES_Y(WORLD_WALL_SEGMENT_HALF, WALL_HALF_HEIGHT)
#define WALL_VERTICES_FLIPPED_Y JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(WORLD_WALL_SEGMENT_HALF, WALL_HALF_HEIGHT)

#define REPEAT_16(V) {V, V, V, V, V, V, V, V, V, V, V, V, V, V, V, V}

// One side of the arena: 16 planes with their world-space centres
typedef struct
{
	jo_3d_quad quads[WORLD_WALL_SEGMENTS];
	jo_pos2D centres[WORLD_WALL_SEGMENTS];
} arena_wall;

// World north is -y and east is -x (see world.h)
static jo_vertice south_vertices[WORLD_WALL_SEGMENTS][4] = REPEAT_16(WALL_VERTICES_FLIPPED_X);
static jo_vertice east_vertices[WORLD_WALL_SEGMENTS][4] = REPEAT_16(WALL_VERTICES_FLIPPED_Y);
static jo_vertice north_vertices[WORLD_WALL_SEGMENTS][4] = REPEAT_16(WALL_VERTICES_X);
static jo_vertice west_vertices[WORLD_WALL_SEGMENTS][4] = REPEAT_16(WALL_VERTICES_Y);

static arena_wall walls[4];

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

// fixed is the coordinate shared by the whole side, step runs along it (+1 or -1)
static void init_wall(arena_wall *wall, jo_vertice vertices[][4], int texture_id, bool along_x, int fixed, int step)
{
	int first = -step * (WORLD_ARENA_HALF - WORLD_WALL_SEGMENT_HALF);

	for (int i = 0; i < WORLD_WALL_SEGMENTS; ++i)
	{
		int along = first + step * i * 2 * WORLD_WALL_SEGMENT_HALF;
		wall->centres[i].x = along_x ? along : fixed;
		wall->centres[i].y = along_x ? fixed : along;
		jo_3d_create_plane(&wall->quads[i], vertices[i]);
		jo_3d_set_texture(&wall->quads[i], texture_id);
	}
}

void init_arena_walls(void)
{
	init_wall(&walls[0], south_vertices, box1_texture_id, true, WORLD_ARENA_HALF, -1);
	init_wall(&walls[1], east_vertices, box2_texture_id, false, -WORLD_ARENA_HALF, 1);
	init_wall(&walls[2], north_vertices, box3_texture_id, true, -WORLD_ARENA_HALF, 1);
	init_wall(&walls[3], west_vertices, box4_texture_id, false, WORLD_ARENA_HALF, -1);
}

void draw_arena_walls(void)
{
	for (int side = 0; side < 4; ++side)
	{
		for (int i = 0; i < WORLD_WALL_SEGMENTS; ++i)
		{
			jo_3d_push_matrix();
			{
				jo_3d_translate_matrix(walls[side].centres[i].x, walls[side].centres[i].y, WALL_CENTRE_Z);
				jo_3d_draw(&walls[side].quads[i]);
			}
			jo_3d_pop_matrix();
		}
	}
}
