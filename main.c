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

#define FLOOR_ORIGIN 1553 // floor texture phase, matches the old world offset
#define PLAYER_HALF 25
#define TRAIL_HEAD_GAP PLAYER_HALF // the wall starts at the back of the cycle

jo_camera cam;
jo_palette image_pal;
Sint16 draw_distance = 10000;

int floor_texture_id = 0;
int hud1_texture_id = 0;
int plane1_texture_id = 0;

bool enable_trails = true;

static trail_mesh player1_trail_mesh;

//Third person
// Cycle: one 8 point mesh drawn with a single slPutPolygon
static POINT cube_points[8];
static POLYGON cube_polygons[6];
static ATTR cube_attributes[6];
static static_mesh cube;

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
	slZdspLevel(0);
}

// The text layer keeps its contents, and jo_printf formatting is costly, so refresh 5 times a second
#define DEBUG_TEXT_INTERVAL 10

void debug_3d(void)
{
	static const char compass[4] = {'N', 'E', 'S', 'W'};
	static int frames_until_refresh = 0;

	if (--frames_until_refresh > 0)
		return;
	frames_until_refresh = DEBUG_TEXT_INTERVAL;

	jo_printf(20, 0, "Compass: %c", compass[player1.heading]);
	jo_printf(20, 1, "Boost: %d  ", player1.gauge);
	jo_printf(0, 0, "x %d    ", player1.position.x);
	jo_printf(0, 1, "y %d    ", player1.position.y);
	jo_printf(0, 2, "view %d %d %d    ", view_x, view_y, view_z);
	jo_printf(0, 3, "segments %d  ", trail_segment_count(&player1_trail));
	jo_printf(0, 4, "%s  ", debug_camera ? "DEBUG CAM" : (first_person ? "1ST" : "ISO"));

	//FPS and 3D counts
	jo_printf(0, 7, "polygons %d   ", jo_3d_get_polygon_count());
	jo_printf(0, 8, "disp_quads %d   ", jo_3d_get_displayed_polygon_count());
	jo_printf(0, 9, "vertices %d   ", jo_3d_get_vertices_count());
	jo_printf(0, 10, "fps %d  cpu %d%% peak %d%%  ", perf_fps, perf_cpu_average, perf_cpu_peak);
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

	//HUD
	//draw_hud();

	// SKY: scrolls with the heading, outside the world matrix
	jo_3d_push_matrix();
	{
		jo_3d_translate_matrix(view_rz / 104, -2, 100); // ANGLE / 104 ~= radians * 100
		jo_background_3d_plane_b_draw(false);
	}
	jo_3d_pop_matrix();

	view_push_world_matrix();
	{
		//  FLOOR
		jo_3d_push_matrix();
		{
			jo_3d_translate_matrix(FLOOR_ORIGIN, FLOOR_ORIGIN, 0);
			jo_background_3d_plane_a_draw(true);
		}
		jo_3d_pop_matrix();

		draw_arena_walls();

		if (enable_trails)
			trail_mesh_draw(&player1_trail_mesh, &player1_trail, TRAIL_HEAD_GAP);

		draw_player();
	}
	jo_3d_pop_matrix();

	debug_3d();
	// debug_pad1();
	perf_frame_end(); // includes the debug text
}

void load_player_model(void)
{
	jo_vertice faces[24] = JO_3D_CUBE_CUSTOM_VERTICES(PLAYER_HALF, PLAYER_HALF, PLAYER_HALF);

	static_mesh_init(&cube, cube_points, 8, cube_polygons, cube_attributes, 6);
	for (int i = 0; i < 6; ++i)
	{
		int polygon = static_mesh_add_quad(&cube, &faces[i * 4]);
		if (polygon >= 0)
			jo_3d_set_mesh_polygon_texture(&cube.mesh, plane1_texture_id, polygon);
	}
}

void draw_player(void)
{
	if (first_person)
		return;
	jo_3d_push_matrix();
	{
		jo_3d_translate_matrix(player1.position.x, player1.position.y, PLAYER_HALF);
		jo_3d_mesh_draw(&cube.mesh);
	}
	jo_3d_pop_matrix();
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
	init_arena_walls();

	load_player_model();
	trail_mesh_init(&player1_trail_mesh, JO_COLOR_RGB(0, 220, 255));

	init_3d_planes();
	initCamera(&cam);
	init_players();

	// Enable low_level_input
	jo_core_add_vblank_callback(operate_digital_pad1);
	jo_core_add_vblank_callback(perf_vblank);
	jo_core_add_callback(perf_frame_start);
	jo_core_add_callback(gamepad_input);
	jo_core_add_callback(draw_3d);

	jo_core_run();
}

/*
 ** END OF FILE
 */
