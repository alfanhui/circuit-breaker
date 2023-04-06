#include "level.h"

const int wall_length = 16;
const int wall_width = 62.5f * wall_length;
const int wall_height = 128;
const int wall_half_width = wall_width /2;
const int wall_half_height = 120; // This is used to riase the wall to the ground

const float division_space_modifier = 1.981f;
const float wall_spacer_modifier = 1.91f;


jo_vertice wall1_vertices[16][4] = {
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_X(wall_width / wall_length, wall_height)};

jo_vertice wall2_vertices[16][4] = {
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_Y(wall_width / wall_length, wall_height)};

jo_vertice wall3_vertices[16][4] = {
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_FLIPPED_VERTICES_X(wall_width / wall_length, wall_height)};

jo_vertice wall4_vertices[16][4] = {
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height),
	JO_3D_PLANE_RECTANGLE_VERTICES_Y(wall_width / wall_length, wall_height)};

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
void init_arena_walls(void)
{
	for (int i = 0; i < wall_length; ++i)
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
	//jo_pos2D position = (jo_pos2D){pos.x - 1500, pos.y - 1500};
	jo_pos2D position = (jo_pos2D){pos.x - 1000, pos.y - 1000};
	
	for (int i = 0; i < wall_length; ++i)
	{
		// 1
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(position.x + ((wall_width * (((float)1 / wall_length) * i) * division_space_modifier)) - wall_width, position.y - wall_width, pos.z + wall_half_height);
			jo_3d_draw(&wall1[i]);
		}
		jo_3d_pop_matrix();
		// 2
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(position.x - wall_width + (wall_width * wall_spacer_modifier), position.y - ((wall_width * (((float)1 / wall_length) * i) * division_space_modifier)) + (wall_width * wall_spacer_modifier) - wall_width, pos.z + wall_half_height);
			jo_3d_draw(&wall2[i]);
		}
		jo_3d_pop_matrix();
		// 3
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(position.x - ((wall_width * (((float)1 / wall_length) * i) * division_space_modifier)) + (wall_width * wall_spacer_modifier) - wall_width, position.y + (wall_width * wall_spacer_modifier) - wall_width, pos.z + wall_half_height);
			jo_3d_draw(&wall3[i]);
		}
		jo_3d_pop_matrix();
		// 4
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(position.x - wall_width, position.y + ((wall_width * (((float)1 / wall_length) * i) * division_space_modifier)) - wall_width, pos.z + wall_half_height);
			jo_3d_draw(&wall4[i]);
		}
		jo_3d_pop_matrix();
	}
}