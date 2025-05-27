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
jo_vertice cube_vertices[] = JO_3D_CUBE_CUSTOM_VERTICES(1000, 1000, 128);
jo_3d_quad cube_quads[6];
Sint16 draw_distance = 500;
jo_pos3Df floorPos = { 0, 0, 50.0};
jo_rot3Df zero_rotf = {0.0,0.0,0.0};
jo_rot3D zero_rot = {0,0,0};
float player_start_pos_x = 0.0;
float player_start_pos_y = 10.0;
int floor_texture_id = 0;
int hud1_texture_id = 0;
int plane1_texture_id = 0;

int increment = 0;
float incrementf = 0.0f;

void initCamera(){

	jo_3d_camera_set_viewpoint(&cam, toFIXED(0), toFIXED(0), toFIXED(0));
	jo_3d_camera_set_z_angle(&cam, DEGtoANG(0)); // orientation
	jo_3d_camera_set_target(&cam, toFIXED(0), toFIXED(0), toFIXED(0));
	jo_3d_perspective_angle(45); // FOV 90
	//jo_3d_display_level(3); // ?
	
	bool window_init = jo_3d_window(0, 0, JO_TV_WIDTH - 1, JO_TV_HEIGHT, draw_distance, JO_TV_WIDTH_2, JO_TV_HEIGHT_2);
	if( window_init == false){
		jo_core_error("values incorrect for 3d_window");
		return;
	}
}

void updateCamera(GameObject* obj)
{
	jo_3d_camera_set_viewpoint(&cam, obj->pos.x, obj->pos.y, obj->pos.z);

	float LOOK_DISTANCE = 1.0f;
	float target_x =  obj->pos.x + jo_cos(0) * LOOK_DISTANCE;
    float target_y = obj->pos.y + jo_sin(0) * LOOK_DISTANCE;
    //jo_3d_camera_set_target(&cam, target_x, target_y , obj->pos.z ); // Look ahead, same height

    // jo_pos3Df target = {
    // 	curCam->viewpoint_pos.x + jo_sin(JO_DEG_TO_RAD(obj->rot.ry)) * look_distance,
    // 	curCam->viewpoint_pos.y,
    //     curCam->viewpoint_pos.z + jo_cos(JO_DEG_TO_RAD(obj->rot.ry)) * look_distance
    // };
    // jo_3d_camera_set_target(curCam, target.x, target.y, target.z);

	//jo_3d_camera_set_target(curCam, obj->pos.x, obj->pos.y, obj->pos.z);
	//jo_3d_camera_set_target(curCam, obj->rot.rx, obj->rot.ry, obj->rot.rz);
}

void init_start_player_location(GameObject* obj){
	obj->pos.x = player_start_pos_x;
	obj->pos.y = player_start_pos_y;
	obj->pos.z = 35;

	obj->rot.rx = JO_DEG_TO_RAD(0.0);
	obj->rot.ry = JO_DEG_TO_RAD(0.0);
	obj->rot.rz = JO_DEG_TO_RAD(1.0); // start a good number
}

void render_object(GameObject* obj) {
    jo_3d_push_matrix();
    // Apply object's world space transformation
    jo_3d_translate_matrix(obj->pos.x, obj->pos.y, obj->pos.z);
    jo_3d_rotate_matrix(obj->rot.rx, obj->rot.ry, obj->rot.rz);
    // Render the object (e.g., a 3D model)
    jo_3d_pop_matrix();
}

void debug_3d(jo_pos3D* pos, jo_rot3D* rot){

	slPrint("pos.x", slLocate(0, 1));
	slPrintFX(pos->x, slLocate(5, 1));
	slPrint("pos.y", slLocate(0, 2));
	slPrintFX(pos->y, slLocate(5, 2));
	slPrint("pos.z", slLocate(0, 3));
	slPrintFX(pos->z, slLocate(5, 3));

	slPrint("rot.rx", slLocate(0, 4));
	slPrintFX(rot->rx, slLocate(6, 4));
	slPrint("rot.ry", slLocate(0, 5));
	slPrintFX(rot->ry, slLocate(6, 5));
	slPrint("rot.rz", slLocate(0, 6));
	slPrintFX(rot->rz, slLocate(6, 6));
}

void debug_3df(jo_pos3Df* pos, jo_rot3Df* rot){

	slPrint("pos.x", slLocate(0, 1));
	slPrintFX(pos->x, slLocate(5, 1));
	slPrint("pos.y", slLocate(0, 2));
	slPrintFX(pos->y, slLocate(5, 2));
	slPrint("pos.z", slLocate(0, 3));
	slPrintFX(pos->z, slLocate(5, 3));

	slPrint("rot.rx", slLocate(0, 4));
	slPrintFX(rot->rx, slLocate(6, 4));
	slPrint("rot.ry", slLocate(0, 5));
	slPrintFX(rot->ry, slLocate(6, 5));
	slPrint("rot.rz", slLocate(0, 6));
	slPrintFX(rot->rz, slLocate(6, 6));
}

void debug_pad1(void)
{
	jo_printf(20, 0, "a.pressed %d", is_key_pressed(DIGI_A));
	jo_printf(20, 1, "a.up %d", is_key_up(DIGI_A));
	jo_printf(20, 2, "a.down %d", is_key_down(DIGI_A));
	jo_printf(20, 3, "key changed %d", is_key_change(DIGI_A));
	jo_printf(20, 4, "a.release %d", is_key_release(DIGI_A));
	jo_printf(20, 5, "a.struck %d", is_key_struck(DIGI_A));
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

void game_loop(void)
{
	increment -= 1;
	incrementf += 0.1f;
	// Attach camera to new object location
	//updateCamera(&lightcycle);
	/*
	* x moves x position
	*/
	jo_3d_camera_set_viewpoint(&cam, 0, 0, 0);

	/*
	* Rx yaw left- right but only on 180 degress
	* Ry pitches top-down but only on 180 degress
	* Rz... not really doing anything
	*/ 
    jo_3d_camera_set_target(&cam, 0, 10, toFIXED(320));
    jo_3d_camera_set_z_angle(&cam, DEGtoANG(0));
	// Apply camera transformation
	jo_3d_camera_look_at(&cam);

	//  FLOOR
	jo_3d_push_matrix();
	{
		jo_3d_rotate_matrix_rad(JO_DEG_TO_RAD(90), 0, 0);
		jo_3d_translate_matrixf(floorPos.x, floorPos.y, floorPos.z - 100);
		jo_background_3d_plane_a_draw(true);
	}
	jo_3d_pop_matrix();

	// SKY
	// jo_3d_push_matrix();
	// {
	// 	jo_3d_rotate_matrix_rad(0, 0, 0);
	// 	jo_3d_translate_matrix(floorPos.x, floorPos.y, floorPos.z); // rot.rx
	// 	jo_background_3d_plane_b_draw(true);
	// }
	// jo_3d_pop_matrix();

	debug_3d(&cam.viewpoint_pos, &zero_rot);
	//debug_3df(&lightcycle.pos, &lightcycle.rot);
	// Walls
	//draw_arena_walls();
}

// void draw_3d(void)
// {
// 	jo_3d_camera_look_at(&cam);
// 	// draw_hud();
// 	//  FLOOR
// 	jo_3d_push_matrix();
// 	{
// 		jo_3d_rotate_matrix_rad(rot.rx, rot.ry, rot.rz);
// 		jo_3d_translate_matrixf(pos.x - 50, pos.y - 5, pos.z);
// 		jo_background_3d_plane_a_draw(true);
// 	}
// 	jo_3d_pop_matrix();

// 	// SKY
// 	jo_3d_push_matrix();
// 	{
// 		jo_3d_translate_matrix(rot.rz * 100, 110 - 112 * jo_cos_radf(0), 100); // rot.rx
// 		jo_background_3d_plane_b_draw(false);
// 	}
// 	jo_3d_pop_matrix();

// 	//draw_arena_walls();
// 	debug_3d();
// 	// debug_pad1();
// }

// //Shoddy attempt at walls with cube
// void create_cube(void)
// {
// 	jo_3d_create_cube(cube_quads, cube_vertices);
// 	jo_3d_set_texture(&cube_quads[1], plane1_texture_id); //up-side-down?
// 	jo_3d_set_texture(&cube_quads[3], plane1_texture_id); //up-side-down?
// 	jo_3d_set_texture(&cube_quads[4], plane1_texture_id);
// 	jo_3d_set_texture(&cube_quads[5], plane1_texture_id);
// }

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
	
	//load_arena_textures();

	init_3d_planes();
	init_start_player_location(&lightcycle);
	jo_3d_camera_init(&cam);
	//initCamera(&cam);

	//create_arena_walls();
	// Enable low_level_input
	jo_core_add_vblank_callback(operate_digital_pad1);

	jo_core_add_callback(gamepad_input3);
	jo_core_add_callback(game_loop);
	
	//jo_core_add_callback(draw_3d);

	jo_core_run();
}

/*
 ** END OF FILE
 */
