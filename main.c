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
jo_vertice cube_vertices[] = JO_3D_CUBE_CUSTOM_VERTICES(1000, 1000, 128); //TODO Remove
jo_3d_quad cube_quads[6];
Sint16 draw_distance = 10000;

int floor_texture_id = 0;
int hud1_texture_id = 0;
int plane1_texture_id = 0;

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
	jo_3d_perspective_angle(90); // FOV 90

	pos.x = 1500;
	pos.y = 1500;
	pos.z = -35.0;

	rot.rx = JO_DEG_TO_RAD(90.0);
	rot.ry = JO_DEG_TO_RAD(0.0);
	rot.rz = JO_DEG_TO_RAD(1.0); // start a good number
}

void debug_3d(void)
{

	int degree = 45;
	float radian = JO_DEG_TO_RAD(degree);

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

	// slPrint("degree = 45", slLocate(0,9));
	// slPrintFX(jo_sin_rad(radian), slLocate(4,10));
	// slPrint("sin", slLocate(4,10));
	// slPrintFX(jo_cos_rad_old(radian), slLocate(4,11));
	// slPrint("old_cos", slLocate(0,11));
	// slPrintFX(jo_cos_rad(radian),slLocate(4,12));
	// slPrint("new_cos", slLocate(0,12));
	// slPrintFX(jo_tan_rad_old(radian), slLocate(4,13));
	// slPrint("old_tan", slLocate(0,13));
	// slPrintFX(jo_tan_rad(radian), slLocate(4,14));
	// slPrint("new_tan", slLocate(0,14));

	// SGL
	//  slPrintFX(slSin(jo_DEGtoANG(degree)), slLocate(24,10));
	//  slPrint("sgl_sin", slLocate(20,10));
	//  slPrintFX(slCos(jo_DEGtoANG(degree)), slLocate(24,11));
	//  slPrint("sgl_cos", slLocate(20,11));
	//  slPrintFX(slTan(jo_DEGtoANG(degree)), slLocate(24,12));
	//  slPrint("sgl_tan", slLocate(20,12));
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

	//draw_arena_walls();
	debug_3d();
	// debug_pad1();
	calculate_player1_trails();
	//draw_player1_trails();
}

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
	load_arena_textures();
	load_player1_trail_textures();

	init_3d_planes();
	initCamera(&cam);

	init_arena_walls();
	//const jo_vertice new_vertice[4] = JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(0, 0, 0, -125, 60); // North
	//const jo_vertice new_vertice[4] = JO_3D_TRAIL_PLANE_VERTICES_Y_NEW(jo_fixed2int(pos.x), jo_fixed2int(pos.x), jo_fixed2int(pos.y + toFIXED(125) ), jo_fixed2int(pos.y), 60); // North
	
	// Enable low_level_input
	jo_core_add_vblank_callback(operate_digital_pad1);

	jo_core_add_callback(gamepad_input);
	jo_core_add_callback(draw_3d);

	jo_core_run();
}

/*
 ** END OF FILE
 */
