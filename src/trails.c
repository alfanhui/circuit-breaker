#include "trails.h"

//jo_pos3Df pos;
//jo_rot3Df rot;
const int plane_divider_length = 125;
const jo_vertice y_trail[] = JO_3D_TRAIL_PLANE_VERTICES_Y(0, plane_divider_length, 60);
const jo_vertice x_trail[] = JO_3D_TRAIL_PLANE_VERTICES_Y(0, plane_divider_length, 60);

static jo_3d_quad player1_trail_quads[200];
const jo_vertice player1_trails[200][4];
static int trail_texture_id = 0;
static int player1_plane_count = 1;
static jo_pos2D player1_previous_coordinate = { 0, 0 }; //Starting position for player1 TODO to change later

void load_player1_trail_textures(void)
{
	trail_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "ORANGE.TGA", JO_COLOR_Transparent);
}

void calculate_player1_trails(void)
{
	//calculate distance
	jo_pos2D distance = { player1_previous_coordinate.x - pos.x, player1_previous_coordinate.y - pos.y };

	if (distance.x == 0 && distance.y == 0){
		//have not moved
		return;
	}else if(distance.y == 0 && ABS(distance.x) > plane_divider_length){
		//moving along x axis
		// jo_3d_create_plane(&player1_trail_quads[player1_plane_count], player1_trails[player1_plane_count]);
		// JO_3D_TRAIL_PLANE_VERTICES_X(wall_width / 16, wall_height);
		//Save position for next time
		player1_previous_coordinate.x = pos.x;
		player1_previous_coordinate.y = pos.y;
	}else if(distance.x == 0 && ABS(distance.y) > plane_divider_length){
		//moving along y axis

		//Save position for next time
		player1_previous_coordinate.x = pos.x;
		player1_previous_coordinate.y = pos.y;
	}

}

void add_player1_x_trails(void){
	jo_3d_create_plane(&player1_trail_quads[--player1_plane_count], x_trail);
	jo_3d_set_texture(&player1_trail_quads[--player1_plane_count], trail_texture_id);
}

void add_player1_y_trails(void){
	jo_3d_create_plane(&player1_trail_quads[--player1_plane_count], y_trail);
	jo_3d_set_texture(&player1_trail_quads[--player1_plane_count], trail_texture_id);
}

void draw_player1_trails(void)
{
	for (int i = 0; i < player1_plane_count; ++i)
	{
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(pos.x, pos.y, pos.z);
			jo_3d_draw(&player1_trail_quads[i]);
		}
		jo_3d_pop_matrix();
	}
}