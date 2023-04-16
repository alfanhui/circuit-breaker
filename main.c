/*
 ** Jo Sega Saturn Engine
 ** Copyright (c) 2012-2020, Johannes Fetz (johannesfetz@gmail.com)
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are met:
 **     * Redistributions of source code must retain the above copyright
 **       notice, this list of conditions and the following disclaimer.
 **     * Redistributions in binary form must reproduce the above copyright
 **       notice, this list of conditions and the following disclaimer in the
 **       documentation and/or other materials provided with the distribution.
 **     * Neither the name of the Johannes Fetz nor the
 **       names of its contributors may be used to endorse or promote products
 **       derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 ** DISCLAIMED. IN NO EVENT SHALL Johannes Fetz BE LIABLE FOR ANY
 ** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 ** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 ** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 ** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "main.h"

jo_camera cam;
jo_palette image_pal;
Sint16 draw_distance = 10000;
bool first_person = false;

int floor_texture_id = 0;
int hud1_texture_id = 0;
int plane1_texture_id = 0;
int last_ticks = 0;

bool enable_trails = true;
bool enable_walls = true;

//Third person
jo_3d_quad cube_quads[6];
jo_vertice cube_vertices[] = JO_3D_CUBE_CUSTOM_VERTICES(25, 25, 25); 

void initCamera(jo_camera *curCam)
{
	(*curCam).viewpoint[X] = toFIXED(0.0);
	(*curCam).viewpoint[Y] = toFIXED(0.0);
	(*curCam).viewpoint[Z] = toFIXED(5.0); // 5.0
	(*curCam).z_angle = DEGtoANG(0.0);	   // orientation
	(*curCam).target[X] = toFIXED(0.0);
	(*curCam).target[Y] = toFIXED(0.0);
	(*curCam).target[Z] = toFIXED(0.0);
	jo_3d_window(0, 0, JO_TV_WIDTH - 1, JO_TV_HEIGHT - 1, draw_distance, JO_TV_WIDTH_2, JO_TV_HEIGHT_2);
	// slZdspLevel(0);
	init_perspective_locations();
}

void debug_3d(void)
{
	slPrint("pos.x", slLocate(0, 0));
	slPrintFX(pos.x, slLocate(5, 0));
	slPrint("pos.y", slLocate(0, 1));
	slPrintFX(pos.y, slLocate(5, 1));
	slPrint("pos.z", slLocate(0, 2));
	slPrintFX(pos.z, slLocate(5, 2));

	slPrint("rot.rx", slLocate(0, 3));
	slPrintFX(rot.rx, slLocate(6, 3));
	slPrint("rot.ry", slLocate(0, 4));
	slPrintFX(rot.ry, slLocate(6, 4));
	slPrint("rot.rz", slLocate(0, 5));
	slPrintFX(rot.rz, slLocate(6, 5));

	//FPS and 3D counts
	//This FPS does not work
	// int ticks       = jo_get_ticks();
    // int delta_ticks = ticks - last_ticks;
    // last_ticks  = ticks;
	// slPrint("fps", slLocate(0,9));
	//slPrintFX(toFIXED(delta_ticks), slLocate(6, 9));
	//slPrintFX(delta_time, slLocate(6, 9));
	slPrint("polygons", slLocate(0,7));
	slPrintFX(toFIXED(jo_3d_get_polygon_count()), slLocate(6,7));
	slPrint("disp_quads", slLocate(0,8));
	slPrintFX(toFIXED(jo_3d_get_displayed_polygon_count()), slLocate(6,8));
	slPrint("vertices", slLocate(0,9));
	slPrintFX(toFIXED(jo_3d_get_vertices_count()), slLocate(6,9));
}

void draw_hud(void)
{
	jo_3d_push_matrix();
	{
		jo_sprite_change_sprite_scale(2);
		jo_sprite_draw3D(hud1_texture_id, 0, 0, 200);
	}
	jo_3d_pop_matrix();
}

void draw_3d(void)
{
	jo_3d_camera_look_at(&cam);
	// draw_hud();
	//  FLOOR
	jo_3d_push_matrix();
	{
		jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
		jo_3d_translate_matrixf(pos.x - 50, pos.y - 5, pos.z);
		jo_background_3d_plane_a_draw(true);
	}
	jo_3d_pop_matrix();

	// SKY
	jo_3d_push_matrix();
	{
		jo_3d_translate_matrix(rot.rz * 100, 110 - 112 * jo_cos_radf(0), 100); // rot.rx
		jo_background_3d_plane_b_draw(false);
	}
	jo_3d_pop_matrix();

	//if(enable_walls){
	draw_arena_walls();
	//}

	debug_3d();
	// debug_pad1();

	//if(enable_trails){
	//draw_player1_trails();
	//calculate_player1_trails();
	//}

	draw_player();
}

void load_player_model(void)
{
	jo_3d_create_cube(cube_quads, cube_vertices);
	jo_3d_set_texture(&cube_quads[0], plane1_texture_id);
	jo_3d_set_texture(&cube_quads[1], plane1_texture_id);
	jo_3d_set_texture(&cube_quads[2], plane1_texture_id);
	jo_3d_set_texture(&cube_quads[3], plane1_texture_id);
	jo_3d_set_texture(&cube_quads[4], plane1_texture_id);
	jo_3d_set_texture(&cube_quads[5], plane1_texture_id);

}

void draw_player(void)
{
	if(!first_person){
		for (int i = 0; i < 6; ++i)
		{
			jo_3d_push_matrix();
			{
				jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
				jo_3d_translate_matrixf(true_position.x - 400, true_position.y - 400, true_position.z);
				jo_3d_draw(&cube_quads[i]);
			}
			jo_3d_pop_matrix();
		}
	}
}

jo_palette *my_tga_palette_handling(void)
{
	// We create a new palette for each image. It's not optimal but OK for a demo.
	jo_create_palette(&image_pal);
	return (&image_pal);
}

void init_3d_planes(void)
{
	jo_img_8bits img;

	jo_core_tv_off();

	jo_enable_background_3d_plane(JO_COLOR_Black);

	// FLOOR
	img.data = JO_NULL;
	jo_tga_8bits_loader(&img, JO_ROOT_DIR, "FLOOR.TGA", 0);
	jo_background_3d_plane_a_img(&img, image_pal.id, true, false);
	jo_free_img(&img);

	// SKY
	img.data = JO_NULL;
	jo_tga_8bits_loader(&img, JO_ROOT_DIR, "SKY.TGA", 0);
	jo_background_3d_plane_b_img(&img, image_pal.id, true, false);
	jo_free_img(&img);

	// Trails
	// if(enable_trails){
	// 	init_player1_x_trails();
	// 	init_player1_y_trails();
	// }

	jo_core_tv_on();
}

void load_textures(void)
{
	floor_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "FLOOR.TGA", JO_COLOR_Transparent);  // spirit 5
	plane1_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALL1.TGA", JO_COLOR_Transparent); // spirit 6
	hud1_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "HUD.TGA", JO_COLOR_Black);		   // spirit 7
}

void jo_main(void)
{
	jo_core_init(JO_COLOR_Black);
	jo_set_tga_palette_handling(my_tga_palette_handling);

	load_textures();

	// if(enable_walls){
	load_arena_textures();
	init_arena_walls();
	// } 

	load_player_model();


	// if(enable_trails){
	// 	load_player1_trail_textures();
	// }

	init_3d_planes();
	initCamera(&cam);
	
	// Enable low_level_input
	jo_core_add_vblank_callback(operate_digital_pad1);

	jo_core_add_callback(gamepad_input);
	jo_core_add_callback(draw_3d);

	jo_core_run();
}

/*
 ** END OF FILE
 */
