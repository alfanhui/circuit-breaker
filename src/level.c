#include "level.h"

const int wall_width = 1000;
const int wall_height = 128;
const int wall_half_width = 500;
const int wall_half_height = 120; // This is used to riase the wall to the ground

const float player_start_pos_x = 500.0f;
const float player_start_pos_y = 500.0f;

jo_vertice plane1_vertices[] = JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width, wall_height);
jo_vertice plane2_vertices[] = JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width, wall_height);
jo_vertice plane3_vertices[] = JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width, wall_height);
jo_vertice plane4_vertices[] = JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width, wall_height);

jo_3d_quad plane1;
jo_3d_quad plane2;
jo_3d_quad plane3;
jo_3d_quad plane4;

int box_texture_id = 0;

void load_arena_textures(void)
{
	box_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX1.TGA", JO_COLOR_Transparent); // spirit 1
}

// Arena walls
void create_arena_walls(void)
{
	jo_3d_create_plane(&plane1, plane1_vertices);
	jo_3d_set_texture(&plane1, box_texture_id);

	jo_3d_create_plane(&plane2, plane2_vertices);
	jo_3d_set_texture(&plane2, box_texture_id);

	jo_3d_create_plane(&plane3, plane3_vertices);
	jo_3d_set_texture(&plane3, box_texture_id);

	jo_3d_create_plane(&plane4, plane4_vertices);
	jo_3d_set_texture(&plane4, box_texture_id);
}

void draw_arena_walls(void)
{
	// 1
	jo_3d_push_matrix();
	{
		jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
		jo_3d_translate_matrixf(pos.x + (wall_width * 2) - (player_start_pos_x * 3), pos.y - wall_width + (player_start_pos_y), pos.z + wall_half_height);
		jo_3d_draw(&plane1);
	}
	jo_3d_pop_matrix();

	// 2
	jo_3d_push_matrix();
	{
		jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
		jo_3d_translate_matrixf(pos.x + wall_width - (player_start_pos_x * 3), pos.y - (player_start_pos_y * 3), pos.z + wall_half_height);
		jo_3d_draw(&plane2);
	}
	jo_3d_pop_matrix();

	// 3
	jo_3d_push_matrix();
	{
		jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
		jo_3d_translate_matrixf(pos.x - (player_start_pos_x * 3), pos.y - wall_width + (player_start_pos_y), pos.z + wall_half_height);
		jo_3d_draw(&plane3);
	}
	jo_3d_pop_matrix();

	// 4
	jo_3d_push_matrix();
	{
		jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
		jo_3d_translate_matrixf(pos.x + wall_width - (player_start_pos_x * 3), pos.y - (wall_width * 2) + (player_start_pos_y * 5), pos.z + wall_half_height);
		jo_3d_draw(&plane4);
	}
	jo_3d_pop_matrix();
}