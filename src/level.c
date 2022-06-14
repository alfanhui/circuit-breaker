#include "level.h"

const int wall_width = 1000;
const int wall_height = 128;
const int wall_half_width = 500;
const int wall_half_height = 120; // This is used to riase the wall to the ground

const float player_start_pos_x = 500.0f;
const float player_start_pos_y = 500.0f;

const float division_space_modifier = 1.981f;
const float wall_spacer_modifier = 1.91f;

jo_vertice wall1_vertices[16][4] = {
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / 16, wall_height)};

jo_vertice wall2_vertices[16][4] = {
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / 16, wall_height)};

jo_vertice wall3_vertices[16][4] = {
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / 16, wall_height)};

jo_vertice wall4_vertices[16][4] = {
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / 16, wall_height)};

jo_3d_quad wall1[16];
jo_3d_quad wall2[16];
jo_3d_quad wall3[16];
jo_3d_quad wall4[16];

int box1_texture_id = 0;
int box2_texture_id = 0;
int box3_texture_id = 0;
int box4_texture_id = 0;

void load_arena_textures(void)
{
	box1_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX1.TGA", JO_COLOR_Transparent); // spirit 1
	box2_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX2.TGA", JO_COLOR_Transparent); // spirit 2
	box3_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX3.TGA", JO_COLOR_Transparent); // spirit 3
	box4_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BOX4.TGA", JO_COLOR_Transparent); // spirit 4
}

// Arena walls
void create_arena_walls(void)
{
	for (int i = 0; i < 16; ++i)
	{
		jo_3d_create_plane(&wall1[i], wall1_vertices[i]);
		jo_3d_set_texture(&wall1[i], box1_texture_id);
		jo_3d_create_plane(&wall2[i], wall2_vertices[i]);
		jo_3d_set_texture(&wall2[i], box2_texture_id);
		jo_3d_create_plane(&wall3[i], wall3_vertices[i]);
		jo_3d_set_texture(&wall3[i], box3_texture_id);
		jo_3d_create_plane(&wall4[i], wall4_vertices[i]);
		jo_3d_set_texture(&wall4[i], box4_texture_id);
	}
}

void draw_arena_walls(void)
{
	for (int i = 0; i < 16; ++i)
	{
		// 1
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(pos.x + ((wall_width * (((float)1 / 16) * i) * division_space_modifier)) - (player_start_pos_x * 3), pos.y - (player_start_pos_y * 3), pos.z + wall_half_height);
			jo_3d_draw(&wall1[i]);
		}
		jo_3d_pop_matrix();
		// 2
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(pos.x - (player_start_pos_x * 3) + (wall_width * wall_spacer_modifier), pos.y - ((wall_width * (((float)1 / 16) * i) * division_space_modifier)) + (wall_width * wall_spacer_modifier) - (player_start_pos_y * 3), pos.z + wall_half_height);
			jo_3d_draw(&wall2[i]);
		}
		jo_3d_pop_matrix();
		// 3
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(pos.x - ((wall_width * (((float)1 / 16) * i) * division_space_modifier)) + (wall_width * wall_spacer_modifier) - (player_start_pos_x * 3), pos.y + (wall_width * wall_spacer_modifier) - (player_start_pos_y * 3), pos.z + wall_half_height);
			jo_3d_draw(&wall3[i]);
		}
		jo_3d_pop_matrix();
		// 4
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(pos.x - (player_start_pos_x * 3), pos.y + ((wall_width * (((float)1 / 16) * i) * division_space_modifier)) - (player_start_pos_y * 3), pos.z + wall_half_height);
			jo_3d_draw(&wall4[i]);
		}
		jo_3d_pop_matrix();
	}
}