#include "trails.h"

jo_pos2D player1_previous_coordinate = {0, 0};
bool create_trail = false;

const int plane_divider_length = 125;
const int plane_divider_length_sub = plane_divider_length + 5; // needs to be slightly bigger to be one
jo_3d_quad player1_quads_x[10];
jo_3d_quad player1_quads_y[10];

static int trail_orange_texture_id = 0;
static int trail_blue_texture_id = 0;
static int player1_plane_x_count_1st = 0;
static int player1_plane_y_count_1st = 0;
static int player1_plane_x_count_3rd = 0;
static int player1_plane_y_count_3rd = 0;
static bool first_frame = true;
static jo_pos2D player1_1st_trails_x_axis[10];
static jo_pos2D player1_1st_trails_y_axis[10];
static jo_pos2D player1_3rd_trails_x_axis[10];
static jo_pos2D player1_3rd_trails_y_axis[10];

jo_vertice trail_plain_y[10][4] = {
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

jo_vertice trail_plain_x[10][4] = {
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
	for (int i = 0; i < 10; i++)
	{
		jo_3d_create_plane(&player1_quads_x[i], trail_plain_x[i]);	
		jo_3d_set_texture(&player1_quads_x[i], trail_orange_texture_id);
	}
}

void init_player1_y_trails()
{
	for (int i = 0; i < 10; i++)
	{
		jo_3d_create_plane(&player1_quads_y[i], trail_plain_y[i]);
		jo_3d_set_texture(&player1_quads_y[i], trail_blue_texture_id);
	}
}

void create_trail_x_1st()
{
	player1_1st_trails_x_axis[player1_plane_x_count_1st] = (jo_pos2D){pos.x, pos.y};
	player1_plane_x_count_1st++;
	player1_1st_trails_x_axis[player1_plane_x_count_1st] = (jo_pos2D){pos.x, pos.y};
}

void create_trail_x_1st_turned()
{
	player1_1st_trails_y_axis[player1_plane_y_count_1st] = (jo_pos2D){pos.x, pos.y};//Finish previous trail
	player1_plane_y_count_1st++;
	player1_plane_x_count_1st++;
	player1_1st_trails_x_axis[player1_plane_x_count_1st] = (jo_pos2D){pos.x, pos.y};
}

void create_trail_y_1st()
{
	player1_1st_trails_y_axis[player1_plane_y_count_1st] = (jo_pos2D){pos.x, pos.y};
	player1_plane_y_count_1st++;
	player1_1st_trails_y_axis[player1_plane_y_count_1st] = (jo_pos2D){pos.x, pos.y};
}

void create_trail_y_1st_turned()
{
	player1_1st_trails_x_axis[player1_plane_x_count_1st] = (jo_pos2D){pos.x, pos.y};//Finish previous trail
	player1_plane_x_count_1st++;
	player1_plane_y_count_1st++;
	player1_1st_trails_y_axis[player1_plane_y_count_1st] = (jo_pos2D){pos.x, pos.y};
}

void create_trail_x_3rd()
{
	player1_3rd_trails_x_axis[player1_plane_x_count_3rd] = (jo_pos2D){true_position.x, true_position.y};
	player1_plane_x_count_3rd++;
	player1_3rd_trails_x_axis[player1_plane_x_count_3rd] = (jo_pos2D){true_position.x, true_position.y};
}

void create_trail_x_3rd_turned()
{
	player1_1st_trails_y_axis[player1_plane_y_count_3rd] = (jo_pos2D){true_position.x, true_position.y};//Finish previous trail
	player1_plane_x_count_3rd++;
	player1_plane_y_count_3rd++;
	player1_1st_trails_x_axis[player1_plane_x_count_3rd] = (jo_pos2D){true_position.x, true_position.y};
}


void create_trail_y_3rd()
{
	player1_3rd_trails_y_axis[player1_plane_y_count_3rd] = (jo_pos2D){true_position.x, true_position.y};
	player1_plane_y_count_3rd++;
	player1_3rd_trails_y_axis[player1_plane_y_count_3rd] = (jo_pos2D){true_position.x, true_position.y};
}

void create_trail_y_3rd_turned()
{
	player1_1st_trails_x_axis[player1_plane_x_count_3rd] = (jo_pos2D){true_position.x, true_position.y};//Finish previous trail
	player1_plane_x_count_3rd++;
	player1_plane_y_count_3rd++;
	player1_1st_trails_y_axis[player1_plane_y_count_3rd] = (jo_pos2D){true_position.x, true_position.y};
}

void debug_trails(jo_pos2D distance){
	slPrint("dist.x: ", slLocate(0, 6));
	slPrintFX(distance.x, slLocate(8, 6));
	slPrint("dist.y: ", slLocate(0, 7));
	slPrintFX(distance.y, slLocate(8, 7));
	slPrint("prev.x: ", slLocate(20, 6));
	slPrintFX(player1_1st_trails_x_axis[player1_plane_x_count_1st].x, slLocate(28, 6));
	slPrint("prev.y: ", slLocate(20, 7));
	slPrintFX(player1_1st_trails_y_axis[player1_plane_y_count_1st].y, slLocate(28, 7));
	slPrint("Xcount: ", slLocate(20, 8));
	slPrintFX(toFIXED(player1_plane_x_count_1st), slLocate(28, 8));
	slPrint("Ycount: ", slLocate(20, 9));
	slPrintFX(toFIXED(player1_plane_y_count_1st), slLocate(28, 9));
}

void calculate_player1_trails(void)
{
	//Skip first frame as no values are ready
	if (first_frame)
	{
		first_frame = !first_frame;
		return;
	}

	// calculate distance
	jo_pos2D distance_1st = (jo_pos2D){0,0};
	jo_pos2D distance_3rd = (jo_pos2D){0,0};

	if (create_trail) //Triggered on turn, force trail create based on previous location
	{
		create_trail = false;
		switch (compass_index)
		{
		case 0: // north
		case 2: // south
			create_trail_y_1st_turned(); //Use x_trail coordinates
			create_trail_y_3rd_turned(); //Use x_trail coordinates
			break;
		case 1: // east
		case 3: // west
			create_trail_x_1st_turned(); //Use y_trail coordinates
			create_trail_x_3rd_turned(); //Use y_trail coordinates
			break;
		}
	}
	else
	{
		switch (compass_index)
		{
		case 0: // north
			distance_1st = (jo_pos2D){0, pos.y - player1_1st_trails_y_axis[player1_plane_y_count_1st].y};
			if (distance_1st.y > plane_divider_length)
			{
				create_trail_y_1st();
			}
			distance_3rd = (jo_pos2D){0, true_position.y - player1_3rd_trails_y_axis[player1_plane_y_count_3rd].y};
			if (distance_3rd.y > plane_divider_length)
			{
				create_trail_y_3rd();
			}
			break;
		case 1: // east
			distance_1st = (jo_pos2D){pos.x - player1_1st_trails_x_axis[player1_plane_x_count_1st].x, 0};
			if (distance_1st.x > plane_divider_length)
			{
				create_trail_x_1st();
			}
			distance_3rd = (jo_pos2D){true_position.x - player1_3rd_trails_x_axis[player1_plane_x_count_3rd].x, 0};
			if (distance_3rd.x > plane_divider_length)
			{
				create_trail_x_3rd();
			}
			break;
		case 2: // south
			distance_1st = (jo_pos2D){0, player1_1st_trails_y_axis[player1_plane_y_count_1st].y - pos.y} ;
			if (distance_1st.y > plane_divider_length)
			{
				create_trail_y_1st();
			}
			distance_3rd = (jo_pos2D){0, player1_3rd_trails_y_axis[player1_plane_y_count_3rd].y - true_position.y} ;
			if (distance_3rd.y > plane_divider_length)
			{
				create_trail_y_3rd();
			}
			break;
		case 3: // west
			distance_1st = (jo_pos2D){player1_1st_trails_x_axis[player1_plane_x_count_1st].x - pos.x, 0};
			if (distance_1st.x > plane_divider_length)
			{
				create_trail_x_1st();
			}
			distance_3rd = (jo_pos2D){player1_3rd_trails_x_axis[player1_plane_x_count_3rd].x - true_position.x, 0};
			if (distance_3rd.x > plane_divider_length)
			{
				create_trail_x_3rd();
			}
			break;
		}
	}

	if (player1_plane_x_count_1st >= 10 )
	{
		player1_plane_x_count_1st = 0;
	}
	if (player1_plane_y_count_1st >= 10 )
	{
		player1_plane_y_count_1st = 0;
	}
	//3rd player
	if (player1_plane_x_count_3rd >= 10 )
	{
		player1_plane_x_count_3rd = 0;
	}
	if (player1_plane_y_count_3rd >= 10 )
	{
		player1_plane_y_count_3rd = 0;
	}
	if (distance_1st.x == 0 && distance_1st.y == 0)
	{
		// have not moved
		return;
	}

	debug_trails(distance_1st);
}

void draw_player1_1st_trails(void)
{
	if (first_frame)
		return;
	for (int i = 0; i < 10; i++)
	{
		if(player1_1st_trails_x_axis[i].x != 0){ //NULL
			jo_3d_push_matrix();
			{
				jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
				jo_3d_translate_matrixf(pos.x - player1_1st_trails_x_axis[i].x, pos.y - player1_1st_trails_x_axis[i].y, pos.z);
				jo_3d_draw(&player1_quads_x[i]);
			}
			jo_3d_pop_matrix();
		}
	}
	for (int i = 0; i < 10; i++)
	{
		if(player1_1st_trails_y_axis[i].y != 0){ //NULL
			jo_3d_push_matrix();
			{
				jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
				jo_3d_translate_matrixf(pos.x - player1_1st_trails_y_axis[i].x, pos.y - player1_1st_trails_y_axis[i].y, pos.z);
				jo_3d_draw(&player1_quads_y[i]);
			}
			jo_3d_pop_matrix();
		}
	}
}


void draw_player1_3rd_trails(void)
{
	if (first_frame)
		return;
	for (int i = 0; i < 10; i++)
	{
		if(player1_1st_trails_x_axis[i].x != 0){ //NULL
			jo_3d_push_matrix();
			{
				jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
				jo_3d_translate_matrixf((true_position.x - player1_3rd_trails_x_axis[i].x)-400, (true_position.y - player1_3rd_trails_x_axis[i].y)-400, true_position.z);
				jo_3d_draw(&player1_quads_x[i]);
			}
			jo_3d_pop_matrix();
		}
	}
	for (int i = 0; i < 10; i++)
	{
		if(player1_1st_trails_y_axis[i].y != 0){ //NULL
			jo_3d_push_matrix();
			{
				jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
				jo_3d_translate_matrixf((true_position.x - player1_3rd_trails_y_axis[i].x)-400, (true_position.y - player1_3rd_trails_y_axis[i].y)-400, true_position.z);
				jo_3d_draw(&player1_quads_y[i]);
			}
			jo_3d_pop_matrix();
		}
	}
}