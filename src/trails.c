#include "trails.h"

jo_pos2D player1_previous_coordinate = {0, 0};
bool create_trail = false;

const int plane_divider_length = 125;
const int plane_divider_length_sub = plane_divider_length + 5; // needs to be slightly bigger to be one
jo_3d_quad player1_quads_x[10];
jo_3d_quad player1_quads_y[10];

static int trail_orange_texture_id = 0;
static int trail_blue_texture_id = 0;
static int player1_plane_x_count = 0;
static int player1_plane_y_count = 0;
static bool first_frame = true;
static jo_pos2D player1_trails_x_coordinates[10];
static jo_pos2D player1_trails_y_coordinates[10];

const jo_vertice trail_plain_y[10][4] = {
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, plane_divider_length_sub, 0, 60)};

const jo_vertice trail_plain_x[10][4] = {
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60),
	JO_3D_TRAIL_PLANE_VERTICES_X_NEW(0, plane_divider_length_sub, 0, 0, 60)};

void load_player1_trail_textures(void)
{
	trail_orange_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "ORANGE.TGA", JO_COLOR_Transparent);
	trail_blue_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "BLUE.TGA", JO_COLOR_Transparent);
}

void init_player1_x_trails()
{
	jo_3d_create_plane(&player1_quads_x[player1_plane_x_count], trail_plain_x[player1_plane_x_count]);
	jo_3d_set_texture(&player1_quads_x[player1_plane_x_count], trail_orange_texture_id);
}

void init_player1_y_trails()
{
	jo_3d_create_plane(&player1_quads_y[player1_plane_y_count], trail_plain_y[player1_plane_y_count]);
	jo_3d_set_texture(&player1_quads_y[player1_plane_y_count], trail_blue_texture_id);
}

void create_trail_x()
{
	init_player1_x_trails();
	player1_plane_x_count++;
	player1_trails_x_coordinates[player1_plane_x_count] = (jo_pos2D){pos.x, pos.y};
}

void create_trail_y()
{
	init_player1_y_trails();
	player1_plane_y_count++;
	player1_trails_y_coordinates[player1_plane_y_count] = (jo_pos2D){pos.x, pos.y};
}

void calculate_player1_trails(void)
{
	if (first_frame)
	{
		first_frame = !first_frame;
		player1_trails_x_coordinates[player1_plane_x_count] = (jo_pos2D){pos.x, pos.y}; // TODO only if we start in the y+ (NORTH) direction
		player1_trails_y_coordinates[player1_plane_y_count] = (jo_pos2D){pos.x, pos.y}; // TODO only if we start in the y+ (NORTH) direction
		return;
	}
	// calculate distance
	jo_pos2D distance;

	if (create_trail) //we've turned, force wall to create
	{
		create_trail != create_trail;
		switch (previous_compass_index)
		{
		case 0: // north
		case 2: // south
			create_trail_y();
			break;
		case 1: // east
		case 3: // west
			create_trail_x();
			break;
		}
	}
	else
	{
		switch (compass_index)
		{
		case 0: // north
			distance = (jo_pos2D){player1_trails_y_coordinates[player1_plane_y_count].x - pos.x, pos.y - player1_trails_y_coordinates[player1_plane_y_count].y};
			if (distance.y > plane_divider_length)
			{
				create_trail_y();
			}
			break;
		case 1: // east
			distance = (jo_pos2D){pos.x - player1_trails_x_coordinates[player1_plane_x_count].x, player1_trails_x_coordinates[player1_plane_x_count].y - pos.y};
			if (distance.x > plane_divider_length)
			{
				create_trail_x();
			}
			break;
		case 2: // south
			distance = (jo_pos2D){player1_trails_y_coordinates[player1_plane_y_count].x - pos.x, player1_trails_y_coordinates[player1_plane_y_count].y - pos.y};
			if (distance.y > plane_divider_length)
			{
				create_trail_y();
			}
			break;
		case 3: // west
			distance = (jo_pos2D){player1_trails_x_coordinates[player1_plane_x_count].x - pos.x, player1_trails_x_coordinates[player1_plane_x_count].y - pos.y};
			if (distance.x > plane_divider_length)
			{
				create_trail_x();
			}
			break;
		}
	}
	slPrint("dist.x: ", slLocate(0, 6));
	slPrintFX(distance.x, slLocate(8, 6));
	slPrint("dist.y: ", slLocate(0, 7));
	slPrintFX(distance.y, slLocate(8, 7));
	slPrint("prev.x: ", slLocate(20, 6));
	slPrintFX(player1_trails_y_coordinates[player1_plane_y_count].x, slLocate(28, 6));
	slPrint("prev.y: ", slLocate(20, 7));
	slPrintFX(player1_trails_y_coordinates[player1_plane_y_count].y, slLocate(28, 7));
	slPrint("tcount: ", slLocate(20, 8));
	slPrintFX(player1_plane_y_count << 16, slLocate(28, 8));

	if (distance.x == 0 && distance.y == 0)
	{
		// have not moved
		return;
	}
	if (player1_plane_x_count >= 10)
	{
		player1_plane_x_count = 0;
	}
	if (player1_plane_y_count >= 10)
	{
		player1_plane_y_count = 0;
	}
}

void draw_player1_trails(void)
{
	if (first_frame)
		return;
	for (int i = 0; i < player1_plane_x_count; i++)
	{
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(pos.x - player1_trails_x_coordinates[i].x, pos.y - player1_trails_x_coordinates[i].y, pos.z);
			jo_3d_draw(&player1_quads_x[i]);
		}
		jo_3d_pop_matrix();
	}
	for (int i = 0; i < player1_plane_y_count; i++)
	{
		jo_3d_push_matrix();
		{
			jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
			jo_3d_translate_matrixf(pos.x - player1_trails_y_coordinates[i].x, pos.y - player1_trails_y_coordinates[i].y, pos.z);
			jo_3d_draw(&player1_quads_y[i]);
		}
		jo_3d_pop_matrix();
	}
}