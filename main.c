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
#include "src/floor_fade_vdp2.h"
#include "src/object_fog_vdp1.h"
#include "src/hud.h"
#include "src/sound.h"
#include "src/game/intro_flight.h"
#include "src/game/arrival_flight.h"

// The floor is VDP2's rotating background, one FLOOR.TGA tile repeated forever (made by tools/make_floor.py).
// Scaling the plane packs FLOOR_TEXELS into FLOOR_TILE_UNITS of world: ~2 texels per unit, so blocks
// near the low 1st person camera stay small.
// The purple-to-pink sky above the walls. It's the second half of the floor's VDP2 screen (RBG0
// plane B), so the floor fog's per-row blend reaches it too, but every row above the horizon
// gets fog level 0 (glow_rows 0), and the 1500 high walls always reach above the horizon, so
// the sky shows only over the wall tops, untouched. false: black beyond the walls instead
#define SHOW_SKY true

// The floor fades to black with distance (src/floor_fade_vdp2.c). It's only been checked in
// emulators: false builds a fallback disc for the real Saturn with the floor at full brightness
// everywhere and the fog's DMA never started (docs/HARDWARE_TESTS.md, test 1)
#define FLOOR_FOG true

// Walls and trails dim with distance too, matching the floor (src/object_fog_vdp1.c), with VDP1
// Gouraud shading. A prototype to judge: Gouraud can only take 16 of 31 steps of brightness off,
// so far walls dim rather than vanish. false: walls and trails at full brightness everywhere
#define WALL_TRAIL_FOG true

// The grid is the player's ruler for speed and distance, so it's the same in every view.
// 125 divides the arena (8000 / 125 = 64 tiles) and the 250 wide wall pieces, so grid lines
// meet the walls cleanly.
#define FLOOR_TEXELS 256
#define FLOOR_TILE_UNITS 125
#define FLOOR_SCALE ((FLOOR_TILE_UNITS << 16) / FLOOR_TEXELS) // FIXED 16.16: world units per texel
// (Spectating, it's drawn larger: view_floor_shift, view.c)
#define PLAYER_HALF 25
#define CYCLE_RADIUS 36 // centre to a corner of the cube, across the floor: 25 x sqrt(2)
#define TRAIL_HEAD_GAP PLAYER_HALF // the wall starts at the back of the cycle

jo_camera cam;
jo_palette image_pal;
// Far clip, in SGL's units: the world reaches SGL at 1/VIEW_WORLD_SCALE size (view.c), so this is
// 30000 world units: the far walls of the largest arena (28000 across) stay visible
Sint16 draw_distance = 30000 / VIEW_WORLD_SCALE;

int hud1_texture_id = 0;
int plane1_texture_id = 0;

bool enable_trails = true;

static trail_mesh player1_trail_mesh;
// One mesh per bot, as each is a colour of its own: the radar's colours after the player's
// (hud.c's map_trail_colours). ~12 KB each (TRAIL_DRAW_COLUMNS_MAX columns)
static trail_mesh bot_trail_meshes[BOT_COUNT_MAX];
static const jo_color bot_trail_colours[BOT_COUNT_MAX] = {
	JO_COLOR_RGB(255, 120, 0),	// orange
	JO_COLOR_RGB(255, 40, 200), // pink
	JO_COLOR_RGB(120, 255, 60), // green
};
static trail_mesh attract_mesh; // the prologue loop's message, every stroke (input.c)
// The cycle's place on screen this frame, in pixels from the middle (+y down): for the burst of
// light when a press hits the prologue's loop (draw_loop_break)
static int burst_x;
static int burst_y;
// 1st person: the trails near the camera that can hide parts of others behind them (game/trail_occlusion.h)
static trail_occluders occluders;

//Third person
// Cycle: one 8 point mesh drawn with a single slPutPolygon. 7 polygons: an anchor inside, then
// the 6 faces (load_player_model)
static POINT cube_points[12];
static POLYGON cube_polygons[7];
static ATTR cube_attributes[7];
static static_mesh cube;

void initCamera(jo_camera *curCam)
{
	(*curCam).viewpoint[X] = toFIXED(0.0);
	(*curCam).viewpoint[Y] = toFIXED(0.0);
	// The eye sits exactly where view.c puts the world's origin (the cycle, in 1st person), looking
	// along -z; view.c's rotations then turn the world in front of it. It was at z 5 looking at
	// the origin: the same direction, but 5 SGL units (20 world units, at 1/4 scale) behind the
	// cycle, while clipping, culling and fog all measure from the cycle. Running beside a trail,
	// its clipped near end then showed as a hole at the screen's edge
	(*curCam).viewpoint[Z] = toFIXED(0.0);
	(*curCam).z_angle = DEGtoANG(0.0);	   // orientation
	(*curCam).target[X] = toFIXED(0.0);
	(*curCam).target[Y] = toFIXED(0.0);
	(*curCam).target[Z] = toFIXED(-5.0); // only the direction matters
	jo_3d_window(0, 0, JO_TV_WIDTH - 1, JO_TV_HEIGHT - 1, draw_distance, JO_TV_WIDTH_2, JO_TV_HEIGHT_2);
	// How close polygons may be drawn: SGL drops anything nearer than its screen distance / 2^level.
	// The world reaches SGL at 1/4 size (view.c), so the 20-40 unit safety distances that
	// wall_clip and trail_clip_columns keep are only 5-10 SGL units: level 7 (the closest, 1/128)
	// allows them. (It was 0, outside the valid 1-7.)
	slZdspLevel(7);
}

// The text layer keeps its contents, and jo_printf formatting is costly, so refresh 5 times a second
#define DEBUG_TEXT_INTERVAL 10

/*
 * The debug readout, top left, 5 lines:
 *     fps 60 cpu 15% pk 28% late 0       frame rate, master SH-2 load (average, worst) and
 *                                        vblanks missed since boot (utils/perf.h)
 *     poly 81/78 vert 92                 polygons sent to SGL / passed on to VDP1, vertices
 *     1ST 0 -5592 N boost 180            the view, the cycle (or the debug camera) and its gauge
 *     spd 7 arena 16000 NOTERM GHOST     tuning, and any debug switch that's on: NOTERM (debug
 *                                        camera Z), GHOST (Y: no collision), LIN (Z: linear
 *                                        turn camera), SLOW (slow motion)
 *     bot -2000 -7011 N boost 72         the bot
 * show: false clears it (the prologue's spectator view, a "main menu", shows none)
 */
#define DEBUG_LINES 5
#define DEBUG_BLANK "                                        " // a whole row: 40 characters

void debug_3d(bool show)
{
	static const char compass[4] = {'N', 'E', 'S', 'W'};
	static int frames_until_refresh = 0;
	static bool shown = false;

	if (!show)
	{
		if (shown)
			for (int row = 0; row < DEBUG_LINES; ++row)
				jo_printf(0, row, DEBUG_BLANK);
		shown = false;
		frames_until_refresh = 0; // straight back when it's shown again
		return;
	}
	shown = true;
	if (--frames_until_refresh > 0)
		return;
	frames_until_refresh = DEBUG_TEXT_INTERVAL;

	// Each line ends in spaces, to cover the end of a longer one printed there before
	jo_printf(0, 0, "fps %d cpu %d%% pk %d%% late %d     ", perf_fps, perf_cpu_average, perf_cpu_peak,
			  perf_late_frames);
	// bots / trl: the worst frame's time for the bots' thinking and riding, and for building the
	// trails' meshes, as % of a frame (utils/perf.h): what the cpu figure is spent on
	jo_printf(0, 1, "poly %d/%d vert %d bots %d%% trl %d%%     ", jo_3d_get_polygon_count(), perf_displayed_polygons,
			  jo_3d_get_vertices_count(), perf_part_peak[PERF_BOTS], perf_part_peak[PERF_TRAILS]);
	if (debug_camera)
		jo_printf(0, 2, "CAM %d %d %d          ", view_x, view_y, view_z);
	else
		jo_printf(0, 2, "%s %d %d %c boost %d     ", first_person ? "1ST" : "ISO", player1.bike.position.x,
				  player1.bike.position.y, compass[player1.bike.heading], player1.bike.gauge);
	jo_printf(0, 3, "spd %d arena %d%s%s%s%s     ", cycle_cruise_speed(), 2 * arena_half(),
			  debug_no_termination ? " NOTERM" : "", player1.bike.bounded ? "" : " GHOST",
			  view_linear_turn ? " LIN" : "", debug_slow_motion ? " SLOW" : "");
	jo_printf(0, 4, "bot %d %d %c boost %d rnd %d.%d %c     ", bots[0].bike.position.x, bots[0].bike.position.y,
			  compass[bots[0].bike.heading], bots[0].bike.gauge, arena_rounds.number + 1, arena_rounds.phase,
			  "EMH"[bot_skill_level]);
}

/*
 * The opening flight's debug line, top left, while it flies (and blanked once it lands):
 *     FLIGHT 2/4 alongside>bank 300 x3
 *     cycle 6400 500 cam 7100 1800 400
 * the segment playing (of 4), the keys it's between (game/intro_flight.c), the frame (of 512)
 * and how hurried it is (input.c); then where the cycle is, and the camera (x, y, height), to
 * place the keys by. false: no lines, as a player would see it
 */
#define SHOW_FLIGHT_DEBUG true

static void debug_flight(void)
{
	static int frames_until_refresh = 0;
	static bool shown = false;
	int frame = view_intro_frame();
	int arriving = view_arrival_frame();

	if (arriving >= 0)
		frame = arriving;
	if (frame < 0 || !SHOW_FLIGHT_DEBUG)
	{
		if (shown)
		{
			jo_printf(0, 0, DEBUG_BLANK);
			jo_printf(0, 1, DEBUG_BLANK);
		}
		shown = false;
		frames_until_refresh = 0;
		return;
	}
	shown = true;
	if (--frames_until_refresh > 0)
		return;
	frames_until_refresh = DEBUG_TEXT_INTERVAL;
	if (arriving >= 0)
	{
		int segment = 0; // the key it's leaving: a few subtractions, not a division

		while (frame >= ARRIVAL_SEGMENT_FRAMES && segment < ARRIVAL_KEYS - 2)
		{
			frame -= ARRIVAL_SEGMENT_FRAMES;
			segment++;
		}
		jo_printf(0, 0, "ARRIVE %d/%d %s>%s %d walls %d          ", segment + 1, ARRIVAL_KEYS - 1,
				  arrival_key_name(segment), arrival_key_name(segment + 1), arriving, view_wall_rise());
		jo_printf(0, 1, "cam %d %d %d     ", view_x, view_y, -view_z);
		return;
	}
	{
		int segment = frame >> INTRO_SEGMENT_SHIFT; // the key it's leaving

		if (segment > INTRO_FLIGHT_KEYS - 2)
			segment = INTRO_FLIGHT_KEYS - 2; // the very last frame: still the last segment
		jo_printf(0, 0, "FLIGHT %d/%d %s>%s %d x%d          ", segment + 1, INTRO_FLIGHT_KEYS - 1,
				  intro_flight_key_name(segment), intro_flight_key_name(segment + 1), frame, intro_hurry);
		jo_printf(0, 1, "cycle %d %d cam %d %d %d     ", player1.bike.position.x, player1.bike.position.y, view_x,
				  view_y, -view_z);
	}
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

// The screens that make up the game: NBG0 (text), NBG1 (jo's bitmap layer) and RBG0 (the floor
// and sky). The terminal screen shows only NBG0: with no polygons sent to VDP1 either, the
// screen is black but for the text. RBG0 is off, and NBG1 is left out too: hidden behind the floor in play, it holds leftover data that
// showed as stripes
#define GAME_SCREENS (JO_NBG0_SCREEN | JO_NBG1_SCREEN | JO_RBG0_SCREEN)
#define TERMINAL_SCREENS JO_NBG0_SCREEN
#define TERMINAL_X 1 // text position, in characters from the top left
#define TERMINAL_Y 2

// The terminal text's colour. jo's printf colours are colour 1 of a palette bank, and jo's own
// green (bank 3) is overwritten by the palettes our images load (the text came out white). A
// palette of our own is a bank nobody else uses (init_terminal)
static jo_palette terminal_palette;

static void init_terminal(void)
{
	jo_create_palette(&terminal_palette);
	jo_set_printf_palette_color(terminal_palette.id, HUD_TERMINAL_GREEN);
}

// After a crash (game/game_state.h): green terminal text typed out on black, behind a block
// cursor. Called every frame while terminated; returns false (drawing nothing) while playing
static bool draw_terminal(void)
{
	static const char text[] = GAME_TERMINAL_TEXT;
	static bool showing = false;
	char typed[sizeof(text)];
	int count;

	if (game.phase != GAME_TERMINATED)
	{
		if (showing)
		{
			// Back to the game: the floor on again, and the text layer cleared for the debug readout
			jo_clear_screen();
			jo_set_displayed_screens(GAME_SCREENS);
			showing = false;
		}
		return false;
	}
	if (!showing)
	{
		jo_clear_screen();
		jo_set_displayed_screens(TERMINAL_SCREENS);
		showing = true;
	}
	// What's typed so far. The text only grows, so the screen needn't be cleared each frame
	count = game_typed_chars(&game);
	for (int i = 0; i < count; ++i)
		typed[i] = text[i];
	typed[count] = 0;
	if (count > 0)
		jo_printf_with_color(TERMINAL_X, TERMINAL_Y, terminal_palette.id, "%s", typed);
	jo_set_printf_color_index(JO_COLOR_INDEX_White); // back to white for the debug readout
	// The cursor sits on the next character's cell
	if (game_cursor_on(&game))
		hud_draw_text_cursor(TERMINAL_X + count, TERMINAL_Y);
	return true;
}

// The status line during play (game/status_message.h): centred above the horizon, in the
// terminal's green behind the same block cursor, so it reads as the same machine talking.
// The text layer keeps what's printed, so it's only printed when another character is typed,
// and blanked once when the message goes
#define STATUS_ROW 13 // text rows are 8 pixels: 104 down, on the dark sky above the horizon (row 17,
					  // just above the radar, sat on the horizon's pink glow: green on pink was hard to read)
#define STATUS_BLANK "                                        " // a whole row: 40 characters

static void draw_status(void)
{
	static int printed = 0; // characters on screen now
	int count = status_typed_chars(&status);
	int column = (40 - status.length) >> 1; // centred on the 40 character row

	if (!status_visible(&status))
	{
		if (printed > 0)
			jo_printf(0, STATUS_ROW, STATUS_BLANK);
		printed = 0;
		return;
	}
	if (count < printed) // a new message over an old one: clear the old first
		jo_printf(0, STATUS_ROW, STATUS_BLANK);
	if (count != printed && count > 0)
	{
		char typed[41];

		for (int i = 0; i < count; ++i)
			typed[i] = status.text[i];
		typed[count] = 0;
		jo_printf_with_color(column, STATUS_ROW, terminal_palette.id, "%s", typed);
		jo_set_printf_color_index(JO_COLOR_INDEX_White); // back to white for the debug readout
	}
	printed = count;
	if (status_cursor_on(&status))
		hud_draw_text_cursor(column + count, STATUS_ROW);
}


// SGL's world-to-screen projection (sl_def.h); jo's headers don't declare it
extern FIXED slConvert3Dto2D(FIXED pos[XYZ], Sint32 ans[XY]);

/*
 * The presses breaking the prologue's loop (game/loop_break.h): the burst of light round the
 * cycle, and the whole screen flashing towards white. The flash is VDP2's colour offset: every
 * screen's colours (the floor, the sprites, the text) have the same amount added, by the hardware,
 * with nothing more to draw. It's turned off again once the flash is over
 */
static void draw_loop_break(void)
{
	static bool flashing = false;
	int flash = loop_break_flash(&breaker);
	int inner;
	int outer;

	if (flash > 0)
	{
		jo_set_screen_color_filter_a(JO_ALL_SCROLL_SCREEN, flash, flash, flash);
		flashing = true;
	}
	else if (flashing)
	{
		jo_disable_screen_color_filter(JO_ALL_SCROLL_SCREEN);
		flashing = false;
	}
	if (loop_break_burst(&breaker, &inner, &outer))
		hud_draw_burst(burst_x, burst_y, inner, outer);
}

void draw_3d(void)
{
	// Terminated: only the terminal text. The world isn't drawn at all
	if (draw_terminal())
	{
		perf_frame_end();
		return;
	}

	jo_3d_camera_look_at(&cam);

	//HUD
	//draw_hud();

	// SKY: scrolls with the heading, outside the world matrix
	if (SHOW_SKY)
	{
		jo_3d_push_matrix();
		{
			jo_3d_translate_matrix(view_rz / 104, -2, 100); // ANGLE / 104 ~= radians * 100
			jo_background_3d_plane_b_draw(false);
		}
		jo_3d_pop_matrix();
	}

	// What the camera can see this frame: walls and trails outside it aren't sent to SGL.
	// Walls are seen across the whole arena; trails only nearby
	world_rect visible_walls = view_visible_rect(&player1.bike, debug_camera, WORLD_WALL_VIEW_RANGE);
	world_rect visible_trails = view_visible_rect(&player1.bike, debug_camera, WORLD_VIEW_RANGE);
	// 1st person: wall and trail corners are kept in front of the camera and inside VDP1's range
	wall_clip_camera clip_camera;
	const wall_clip_camera *clip = view_wall_clip_camera(&player1.bike, debug_camera, &clip_camera) ? &clip_camera : JO_NULL;

	// Fog for walls and trails: from where the camera stands, the way it faces this frame
	if (object_fog_enabled)
	{
		world_point fog_eye;
		int fog_forward_x;
		int fog_forward_y;
		bool fog_on = view_fog_camera(debug_camera, &fog_eye, &fog_forward_x, &fog_forward_y);
		object_fog_begin_frame(fog_on, fog_eye, fog_forward_x, fog_forward_y);
	}

	view_push_world_matrix();
	{
		//  FLOOR
		jo_3d_push_matrix();
		{
			const FIXED floor_scale = FLOOR_SCALE << view_floor_shift;

			slScale(floor_scale, floor_scale, JO_FIXED_1); // only x and y: the plane is flat
			jo_background_3d_plane_a_draw(true);
		}
		jo_3d_pop_matrix();

		// Spectating: no walls, so the arena seems to go on for ever (view.c). They rise out of the
		// floor as the camera comes down into the cycle; meanwhile they're cut finer near the camera
		// (its spot on the floor), not near the cycle
		if (!spectating)
		{
			world_point wall_eye = player1.bike.position;

			if (view_arriving())
			{
				wall_eye.x = (short)view_x;
				wall_eye.y = (short)view_y;
			}
			draw_arena_walls(&visible_walls, &wall_eye, clip, !first_person && !debug_camera, arena_wall_rise());
			// The ground round the arena, raised level with the walls' tops: the arena's sunk into
			// it, and the floor's grid is only seen inside. It rises with the walls, from the
			// break's flash (flat on the floor till then, hiding the grid outside). Between rounds,
			// once it's sunk flat, it goes: the grid runs on for ever, as in the loop
			if (arena_rounds.phase != ROUND_OPEN)
				draw_arena_surround(arena_wall_rise(), !first_person && !debug_camera);
		}

		if (enable_trails)
		{
			int trails_frc = jo_time_get_frc(); // the debug line's "trl" figure (utils/perf.h)
			// 1st person (the eye is below the trails' tops): a part of a trail hidden behind a
			// nearer one isn't drawn. VDP1 has no depth buffer, and side by side pieces of two
			// parallel trails sort in either order: the hidden one showed through
			const trail_occluders *hide = JO_NULL;

			if (clip)
			{
				trail_occluders_begin(&occluders, clip->eye);
				// Only trails standing full height: a sinking one is lower than the eye, hiding nothing
				for (int i = 0; i < solid_trail_count; ++i)
					trail_occluders_add(&occluders, solid_trails[i], TRAIL_HEAD_GAP, &visible_trails);
				hide = &occluders;
			}
			trail_mesh_draw(&player1_trail_mesh, &player1.wall, player1.trail_height, TRAIL_HEAD_GAP, player1.bike.position,
							&visible_trails, clip, hide);
			// Cut finer near the player (the camera), not near the bots: that's where detail shows.
			// A dead bot's trail sinks (its height falls), then has nothing left to draw
			for (int i = 0; i < BOT_COUNT_MAX; ++i)
				if (bots[i].trail_height > 0)
					trail_mesh_draw(&bot_trail_meshes[i], &bots[i].wall, bots[i].trail_height, TRAIL_HEAD_GAP,
									player1.bike.position, &visible_trails, clip, hide);
			// The prologue's loop: the finished strokes of its message, each as tall as its word
			// stands (sinking once it's spelt). All of them in one mesh, drawn once (trail_render.h)
			trail_mesh_begin(&attract_mesh);
			for (int i = 0; i < attract_stroke_count; ++i)
			{
				int height = attract_word_height[attract_stroke_word[i]];

				if (height > 0)
					trail_mesh_add(&attract_mesh, &attract_strokes[i], height, 0, player1.bike.position,
								   &visible_trails, clip, hide);
			}
			trail_mesh_end(&attract_mesh);
			perf_part_add(PERF_TRAILS, trails_frc);
		}

		// In 1st person the camera is inside it; coming down into it, it's seen until the camera's close
		if (!first_person || spectating ||
			(view_arriving() && JO_ABS(view_x - player1.bike.position.x) + JO_ABS(view_y - player1.bike.position.y) > 150))
			draw_cycle(player1.bike.position);
		// Where the cycle is on screen, for the burst of light round it (draw_loop_break): SGL
		// works it out through the camera, so it has to be asked in here. In 1st person the
		// camera's inside it: the middle of the screen
		burst_x = 0;
		burst_y = 0;
		if (spectating || view_arriving())
		{
			// Coming down, the last burst stays where the cycle was when the loop broke
			world_point at = spectating ? player1.bike.position : loop_break_at;
			FIXED middle[XYZ] = {jo_int2fixed(at.x), jo_int2fixed(at.y), jo_int2fixed(PLAYER_HALF)};
			Sint32 on_screen[XY];

			slConvert3Dto2D(middle, on_screen);
			// Kept on the screen: in the opening flight the cycle can be off it (or behind the
			// camera), and a burst drawn round a point far outside it would wrap into spikes
			burst_x = JO_MAX(-JO_TV_WIDTH_2, JO_MIN(JO_TV_WIDTH_2, on_screen[X]));
			burst_y = JO_MAX(-JO_TV_HEIGHT_2, JO_MIN(JO_TV_HEIGHT_2, on_screen[Y]));
		}
		// 1st person: only if it's wholly in front of the camera (SGL would put one behind the
		// camera on screen anyway, as if in front) and not wholly off to one side
		for (int i = 0; i < BOT_COUNT_MAX; ++i)
			if (bots[i].alive &&
				(!clip || wall_clip_object_visible(clip, bots[i].bike.position, CYCLE_RADIUS, TRAIL_MIN_FORWARD)))
				draw_cycle(bots[i].bike.position);
	}
	jo_3d_pop_matrix();

	draw_loop_break();
	// Spectating, you're only watching: no HUD (the gauge, the radar and the status line), nor
	// while the camera comes down into the cycle: it appears as you take over
	if (spectating || view_arriving())
	{
		debug_3d(false);
		debug_flight();
		perf_frame_end();
		return;
	}
	hud_draw_boost(&player1.bike); // on top of the world: screen pixels, no perspective
	if (!debug_camera) // both views: in iso it turns with the iso camera (hud.h)
	{
		// Every trail on the field, and a marker on the rim pointing at each live bot
		hud_marker markers[BOT_COUNT_MAX];
		int marker_count = 0;

		for (int i = 0; i < BOT_COUNT_MAX; ++i)
			if (bots[i].alive)
			{
				markers[marker_count].position = bots[i].bike.position;
				markers[marker_count].colour = (unsigned char)(1 + i); // its place in field_trails: its colour
				marker_count++;
			}
		hud_draw_minimap(player1.bike.position, field_trails, FIELD_TRAIL_COUNT, markers, marker_count);
	}
	draw_status();
	debug_3d(true);
	// debug_pad1();
	perf_frame_end(); // includes the debug text
}

// A face of the cube points out along the one axis on which its 4 corners all agree: that
// value's sign is which way. SGL needs it to tell front faces from back (Single_Plane)
static void set_outward_normal(POLYGON *polygon, const jo_vertice corners[4])
{
	polygon->norm[X] = JO_FIXED_0;
	polygon->norm[Y] = JO_FIXED_0;
	polygon->norm[Z] = JO_FIXED_0;
	if (corners[0].x == corners[1].x && corners[0].x == corners[2].x)
		polygon->norm[X] = corners[0].x > 0 ? JO_FIXED_1 : -JO_FIXED_1;
	else if (corners[0].y == corners[1].y && corners[0].y == corners[2].y)
		polygon->norm[Y] = corners[0].y > 0 ? JO_FIXED_1 : -JO_FIXED_1;
	else
		polygon->norm[Z] = corners[0].z > 0 ? JO_FIXED_1 : -JO_FIXED_1;
}

/*
 * The cycle is drawn as one object at one depth: its centre's.
 * VDP1 has no depth buffer, so SGL draws polygons back to front, each judged by its own centre
 * (SORT_CEN). Judged face by face, the cube's top (50 up) counted as nearer than a trail right
 * beside it on the camera's side, and was drawn over the trail that should hide it.
 * So: first an anchor, a square across the middle of the cube whose centre is the cube's
 * centre, sorted as usual. Then the 6 faces with SORT_BFR, "where the last polygon was": all
 * drawn just after the anchor, at its depth. The anchor is inside the cube, so its own faces
 * always cover it. With every face at the same depth their order no longer sorts itself out, so
 * the faces are single-sided: back faces are skipped, and the 3 front ones never overlap.
 */
void load_player_model(void)
{
	jo_vertice faces[24] = JO_3D_CUBE_CUSTOM_VERTICES(PLAYER_HALF, PLAYER_HALF, PLAYER_HALF);
	jo_vertice anchor[4] = {
		{-PLAYER_HALF, -PLAYER_HALF, 0},
		{PLAYER_HALF, -PLAYER_HALF, 0},
		{PLAYER_HALF, PLAYER_HALF, 0},
		{-PLAYER_HALF, PLAYER_HALF, 0},
	};
	int polygon;

	static_mesh_init(&cube, cube_points, 12, cube_polygons, cube_attributes, 7);
	polygon = static_mesh_add_quad(&cube, anchor);
	if (polygon >= 0)
		jo_3d_set_mesh_polygon_texture(&cube.mesh, plane1_texture_id, polygon); // sorted by its centre (SORT_CEN)
	for (int i = 0; i < 6; ++i)
	{
		polygon = static_mesh_add_quad(&cube, &faces[i * 4]);
		if (polygon < 0)
			continue;
		jo_3d_set_mesh_polygon_texture(&cube.mesh, plane1_texture_id, polygon);
		set_outward_normal(&cube_polygons[polygon], &faces[i * 4]);
		cube_attributes[polygon].flag = Single_Plane;
		// The texture setter wrote SORT_CEN into the low 2 bits: swap it for SORT_BFR
		cube_attributes[polygon].sort = (cube_attributes[polygon].sort & ~3) | SORT_BFR;
	}
}

// A cycle (the cube for now) standing at position
void draw_cycle(world_point position)
{
	jo_3d_push_matrix();
	{
		jo_3d_translate_matrix(position.x, position.y, PLAYER_HALF);
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
	if (SHOW_SKY)
	{
		img.data = JO_NULL;
		jo_tga_8bits_loader(&img, JO_ROOT_DIR, "SKY.TGA", 0);
		jo_background_3d_plane_b_img(&img, image_pal.id, true, false);
		jo_free_img(&img);
	}

	// The far floor fades to black. Skipped, nothing else in the fog runs either: its other
	// functions (view changes, the VBlank restart) return early until this has set it up
	if (FLOOR_FOG)
		floor_fade_init();

	jo_core_tv_on();
}

void load_textures(void)
{
	// FLOOR.TGA is only used by the VDP2 floor (init_3d_planes); as a VDP1 sprite it would waste 64 KB
	plane1_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "WALL1.TGA", JO_COLOR_Transparent); // spirit 6
	hud1_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "HUD.TGA", JO_COLOR_Black);		   // spirit 7
}

#ifdef SIMULATE_DIRTY_RAM
// Emulators power on with zeroed RAM; real hardware (and SAROO's menu) leaves old data behind.
// Fill the part of BSS that jo's startup fails to clear with junk, to reproduce that here.
extern unsigned char _bstart;
extern unsigned char _bend;

static void simulate_dirty_ram(void)
{
	unsigned char *p = &_bstart + (&_bend - &_bstart) / 4; // jo's main() only clears up to here
	while (p < &_bend)
		*p++ = 0xA5;
#ifdef SIMULATE_FIXED_CLEAR
	jo_memset(&_bstart, 0, &_bend - &_bstart); // byte count: what main() should have done
#endif
}
#endif

void jo_main(void)
{
#ifdef SIMULATE_DIRTY_RAM
	simulate_dirty_ram();
#endif
	jo_core_init(JO_COLOR_Black);
	jo_set_tga_palette_handling(my_tga_palette_handling);

	load_textures();

	load_arena_textures();
	hud_init();
	sound_init();
	// After the textures (it checks they leave room for its tables), before the walls and trails
	// (they set their quads up for fog only if it's on)
	if (WALL_TRAIL_FOG)
		object_fog_init();
	init_arena_walls();

	load_player_model();
	trail_mesh_init(&player1_trail_mesh, JO_COLOR_RGB(0, 220, 255));
	for (int i = 0; i < BOT_COUNT_MAX; ++i)
		trail_mesh_init(&bot_trail_meshes[i], bot_trail_colours[i]); // the radar's colours (hud.c)
	trail_mesh_init(&attract_mesh, JO_COLOR_RGB(0, 220, 255)); // the player's: it's writing it

	init_3d_planes();
	init_terminal(); // after every image palette, so its bank is one of its own
	initCamera(&cam);
	init_players();

	// Enable low_level_input
	jo_core_add_vblank_callback(operate_digital_pad1);
	jo_core_add_vblank_callback(floor_fade_vblank); // restarts the per-row fog each frame
	jo_core_add_callback(perf_frame_start);
	jo_core_add_callback(gamepad_input);
	jo_core_add_callback(draw_3d);

	jo_core_run();
}

/*
 ** END OF FILE
 */
