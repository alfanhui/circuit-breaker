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
#include "src/utils/angles.h"
#include "src/floor_fade_vdp2.h"
#include "src/object_fog_vdp1.h"
#include "src/hud.h"
#include "src/scene_memory.h"
#include "src/sound.h"
#include "src/drone_draw.h"
#include "src/slave_draw.h"
#include "src/game/intro_flight.h"
#include "src/game/landmarks.h"
#include "src/game/board.h"
#include "src/game/arrival_flight.h"
#include "src/game/draw_budget.h" // what a frame leaves out when VDP1 is short (SG-8)
#include "src/features.h" // the build's switches: SHOW_SKY, FLOOR_FOG, CYCLE_SHAPE ...
#include "src/game/settings.h" // the pause menu's: the ones that can change while playing

// What this frame leaves out when VDP1 couldn't finish the one before (game/draw_budget.h, SG-8)
static draw_budget budget = {0, 0, BUDGET_HOLD_FRAMES, -1};

// The cycle's shape now (CYCLE_RIBBON ... CYCLE_LINKED): the pause menu's, CYCLE_SHAPE at boot
static inline int cycle_shape(void)
{
	return settings_value(&game_settings, SETTING_CYCLE_SHAPE);
}

// The floor is VDP2's rotating background, one FLOOR.TGA tile repeated forever (made by tools/make_floor.py).
// Scaling the plane packs FLOOR_TEXELS into FLOOR_TILE_UNITS of world: ~2 texels per unit, so blocks
// near the low 1st person camera stay small.

// The grid is the player's ruler for speed and distance, so it's the same in every view.
// 125 divides the arena (8000 / 125 = 64 tiles) and the 250 wide wall pieces, so grid lines
// meet the walls cleanly.
#define FLOOR_TEXELS 256
#define FLOOR_TILE_UNITS 125
#define FLOOR_SCALE ((FLOOR_TILE_UNITS << 16) / FLOOR_TEXELS) // jo_fixed 16.16: world units per texel

/*
 * The floor's matrix, for VDP2 (jo-engine 2026). jo turns it into VDP2's rotation parameters:
 * entries A-F of its matrix are 4 x the texels per matrix unit (jo's vdp2.c,
 * jo_vdp2_set_rotation_matrix), and VDP2 keeps only a sign, 3 integer bits and 10 bits of
 * fraction of each: under 8, or it wraps. Under SGL the floor was drawn in the world's matrix, at
 * 1/VIEW_WORLD_SCALE size: 2.048 texels a world unit made 32.8 texels a matrix unit, and on jo
 * that wrapped, a floor ~10 times too big that changed with the view (2026-11-08).
 * So the floor has the world at 17/16 its size round the camera instead: 2.048 / (17/16) = 1.93
 * texels a matrix unit, 7.7 in VDP2's entries. VIEW_WORLD_SCALE is only
 * for VDP1's depths (view.c).
 * Not larger either: the bigger the world in the matrix, the deeper the floor in it, and jo looks
 * up the floor's zoom with depth (VDP2's coefficient table, 128 / address): far off, a big zoom
 * comes in coarse steps, and whole bands of rows jumped. Seen from the opening flight's heights
 * (thousands of units away) the grid broke into dashes. So drawn larger (view_floor_shift, its
 * texels bigger), the world's shrunk by as much: the texels a matrix unit stay 1.93, the depths
 * get smaller. Then there's the limit on its position:
 */
#define FLOOR_VIEW_SCALE ((JO_FIXED_1 * VIEW_WORLD_SCALE * 17) >> 4) // x 4.25: the world's 1/4 undone, x 17/16
/*
 * ...where the camera is, in texels: VDP2 keeps 13 integer bits (under 8192 texels, 4000 world
 * units), and the arena's 28000 across. The grid repeats every tile, so only the camera's place
 * within some whole number of tiles matters: FLOOR_WRAP, 16 of them (2000 units, ~3900 texels)
 */
#define FLOOR_WRAP (16 * FLOOR_TILE_UNITS)

// v within the grid's repeat: 0 to FLOOR_WRAP. By subtraction, not %: the arena's at most ~15
// repeats across, so a few steps a frame, and no division (CLAUDE.md)
static int floor_wrap(int v)
{
	// Drawn larger than 16 times (view_floor_shift over 4: the board under the city), one tile's
	// wider than FLOOR_WRAP: the repeat's a whole tile then, or the floor would jump
	const int wrap = view_floor_shift > 4 ? FLOOR_WRAP << (view_floor_shift - 4) : FLOOR_WRAP;

	while (v >= wrap)
		v -= wrap;
	while (v < 0)
		v += wrap;
	return v;
}
// (Spectating, it's drawn larger: view_floor_shift, view.c)
#define PLAYER_HALF 25
#define CYCLE_RADIUS 36 // centre to a corner of the cube, across the floor: 25 x sqrt(2)
#define TRAIL_HEAD_GAP PLAYER_HALF // the wall starts at the back of the cycle

jo_camera cam;
jo_palette image_pal;
// The floor's own palette (each image loads into a new one: the sky's comes after it, into
// image_pal), and its colours as loaded, so it can go white or black and come back (set_floor_colours)
#define FLOOR_COLOURS 256 // FLOOR.TGA is 8-bit
static jo_palette floor_pal;
static jo_color floor_colours[FLOOR_COLOURS];
// Far clip, in SGL's units: the world reaches SGL at 1/VIEW_WORLD_SCALE size (view.c), so this is
// 30000 world units: the far walls of the largest arena (28000 across) stay visible
short draw_distance = 30000 / VIEW_WORLD_SCALE;

int hud1_texture_id = 0;
int plane1_texture_id = 0;
static int plane1_palette_id = 0; // WALL1.TGA is 8-bit: the palette it loaded with (load_textures)

bool enable_trails = true;

// The trails' meshes: the arena's memory (scene_memory.h), NULL once it's given back. One mesh
// per bot, as each is a colour of its own: the radar's colours after the player's (hud.c's
// map_trail_colours). ~12 KB each (TRAIL_DRAW_COLUMNS_MAX columns)
static trail_mesh *player1_trail_mesh = JO_NULL;
static trail_mesh *bot_trail_meshes = JO_NULL; // BOT_COUNT_MAX of them
static const jo_color bot_trail_colours[BOT_COUNT_MAX] = {
	JO_COLOR_RGB(255, 120, 0),	// orange
	JO_COLOR_RGB(255, 40, 200), // pink
	JO_COLOR_RGB(120, 255, 60), // green
};
static trail_mesh *attract_mesh = JO_NULL; // the prologue loop's message, every stroke (input.c)

void arena_trail_meshes_take(void)
{
	trail_mesh *meshes;

	if (player1_trail_mesh != JO_NULL)
		return;
	// All in one: the player's, the bots', the message's
	meshes = (trail_mesh *)jo_malloc(sizeof(trail_mesh) * (BOT_COUNT_MAX + 2));
	if (meshes == JO_NULL)
	{
		jo_core_error("No memory for the trails");
		return;
	}
	player1_trail_mesh = &meshes[0];
	bot_trail_meshes = &meshes[1];
	attract_mesh = &meshes[1 + BOT_COUNT_MAX];
	trail_mesh_init(player1_trail_mesh, JO_COLOR_RGB(0, 220, 255));
	for (int i = 0; i < BOT_COUNT_MAX; ++i)
		trail_mesh_init(&bot_trail_meshes[i], bot_trail_colours[i]); // the radar's colours (hud.c)
	trail_mesh_init(attract_mesh, JO_COLOR_RGB(0, 220, 255)); // the player's: it's writing it
}

void arena_trail_meshes_release(void)
{
	if (player1_trail_mesh == JO_NULL)
		return;
	jo_free(player1_trail_mesh); // the start of the one block
	player1_trail_mesh = JO_NULL;
	bot_trail_meshes = JO_NULL;
	attract_mesh = JO_NULL;
}
// The cycle's place on screen this frame, in pixels from the middle (+y down): for the burst of
// light when a press hits the prologue's loop (draw_loop_break)
static int burst_x;
static int burst_y;
// 1st person: the trails near the camera that can hide parts of others behind them (game/trail_occlusion.h)
static trail_occluders occluders;

//Third person
// A cycle: one model for every rider (load_player_model has its shape), 10 points and 5 polygons
#define PLAYER_COLOUR JO_COLOR_RGB(0, 220, 255) // the player's trail, and cycle
// A quarter turn for the model, as jo's matrix turns it (0x4000 is a quarter of 0x10000). Its sign
// is which way round a heading goes: east is -x (game/world.h)
#define RIBBON_QUARTER_TURN (-0x4000)
#define RIBBON_PARTS 4					// the twist's strips
#define RIBBON_EDGE RIBBON_PARTS		// then the leading edge's line
#define RIBBON_POINTS ((RIBBON_PARTS + 1) * 2)
static jo_pos3D_fixed ribbon_points[RIBBON_POINTS];
static jo_3d_polygon ribbon_polygons[RIBBON_PARTS + 1];
static jo_3d_polygon_attribute ribbon_attributes[RIBBON_PARTS + 1];
static jo_3d_mesh ribbon;
// The token: 6 points (its foot, four round the middle, its top), 8 three-cornered faces
#define TOKEN_POINTS 6
#define TOKEN_FACES 8
static jo_pos3D_fixed token_points[TOKEN_POINTS];
static jo_3d_polygon token_polygons[TOKEN_FACES];
static jo_3d_polygon_attribute token_attributes[TOKEN_FACES];
static jo_3d_mesh token;
// The cursor: a slab leaning forwards, 8 points, 5 faces (no underside)
#define CURSOR_POINTS 8
#define CURSOR_FACES 5
static jo_pos3D_fixed cursor_points[CURSOR_POINTS];
static jo_3d_polygon cursor_polygons[CURSOR_FACES];
static jo_3d_polygon_attribute cursor_attributes[CURSOR_FACES];
static jo_3d_mesh cursor_block;

void initCamera(jo_camera *curCam)
{
	// (viewpoint_pos and target_pos: jo_camera's points as x, y, z, 16.16)
	curCam->viewpoint_pos.x = JO_FIXED_0;
	curCam->viewpoint_pos.y = JO_FIXED_0;
	// The eye sits exactly where view.c puts the world's origin (the cycle, in 1st person), looking
	// along -z; view.c's rotations then turn the world in front of it. It was at z 5 looking at
	// the origin: the same direction, but 5 SGL units (20 world units, at 1/4 scale) behind the
	// cycle, while clipping, culling and fog all measure from the cycle. Running beside a trail,
	// its clipped near end then showed as a hole at the screen's edge
	curCam->viewpoint_pos.z = JO_FIXED_0;
	curCam->z_angle = 0; // orientation
	curCam->target_pos.x = JO_FIXED_0;
	curCam->target_pos.y = JO_FIXED_0;
	curCam->target_pos.z = jo_int2fixed(-5); // only the direction matters
	jo_3d_window(0, 0, JO_TV_WIDTH - 1, JO_TV_HEIGHT - 1, draw_distance, JO_TV_WIDTH_2, JO_TV_HEIGHT_2);
	// How close polygons may be drawn: jo (as SGL did) drops anything nearer than its screen
	// distance / 2^level. The world reaches jo's 3D at 1/4 size (view.c), so the 20-40 unit safety
	// distances that wall_clip and trail_clip_columns keep are only 5-10 units: level 7 (the
	// closest, 1/128) allows them
	jo_3d_display_level(7);
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
 *     ar3 co9 sd14 tp3 fl9 gr2 bd1 wk0   the level's parts, each its worst frame in the last
 *                                        second as % of a frame (utils/perf.h): the arena's walls,
 *                                        the corridor's, the boxes' sides, their tops, the glass
 *                                        floor, the VDP2 grid, SGL taking the walls' strips, the
 *                                        walker. cpu less these
 *                                        is the rest; low cpu with low fps is VDP1 (docs/CULLING.md)
 * show: false clears it (the prologue's spectator view, a "main menu", shows none)
 */
#define DEBUG_LINES 10 // rows 0 to 9 (mwait sidle is on row 8, VDP1's pixels on 9)
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
			  settings_on(&game_settings, SETTING_NO_TERMINATION) ? " NOTERM" : "", player1.bike.bounded ? "" : " GHOST",
			  view_linear_turn ? " LIN" : "", debug_slow_motion ? " SLOW" : "");
	jo_printf(0, 4, "bot %d %d %c boost %d rnd %d.%d %c     ", bots[0].bike.position.x, bots[0].bike.position.y,
			  compass[bots[0].bike.heading], bots[0].bike.gauge, arena_rounds.number + 1, arena_rounds.phase,
			  "EMH"[bot_skill_now()]);
	jo_printf(0, 8, "mwait %d sidle %d   ", jo_core_master_wait_time(), jo_core_slave_idle_time());
#if CITY_WATCHDOG
	// The watchdogs (game/drone.h): each one's mood (P patrol, R returning to it, C chase, S search)
	// and its catch count (caught at DRONE_CATCH_FRAMES)
	if (view_is_on_foot())
	{
		static const char mood[3] = {'P', 'C', 'S'};

		jo_printf(24, 8, "wd");
		for (int i = 0; i < CITY_WATCHDOGS && i < 3; ++i)
			jo_printf(27 + i * 4, 8, "%c%-3d", watchdogs[i].returning ? 'R' : mood[watchdogs[i].mode],
					  watchdogs[i].held);
	}
#endif
#ifdef JO_COMPILE_WITH_PROFILER_SUPPORT
	// VDP1's pixels, thousands, worst frame (utils/perf.h): the whole, the slave's (the city's floor
	// and boxes), then the master's: world, arena, trails, sky, drone, body, mirror, the rest
	jo_printf(0, 9, "px%d S%d w%d a%d t%d k%d d%d b%d m%d o%d   ", perf_px_peak[PX_TOTAL], perf_px_peak[PX_SLAVE],
			  perf_px_peak[PX_WORLD], perf_px_peak[PX_ARENA], perf_px_peak[PX_TRAILS], perf_px_peak[PX_SKY],
			  perf_px_peak[PX_DRONE], perf_px_peak[PX_BODY], perf_px_peak[PX_MIRROR], perf_px_peak[PX_OTHER]);
#endif
	jo_printf(0, 5, "ar%d co%d sd%d tp%d fl%d gr%d bd%d wk%d mi%d     ", perf_part_peak[PERF_ARENA],
			  perf_part_peak[PERF_CORRIDOR], perf_part_peak[PERF_SIDES], perf_part_peak[PERF_TOPS],
			  perf_part_peak[PERF_FLOOR], perf_part_peak[PERF_GRID], perf_part_peak[PERF_BANDS],
			  perf_part_peak[PERF_WALKER], perf_part_peak[PERF_MIRROR]);
	// The drone (row 5's full): flying and looking, its model, its scan's sheet
	// ...and bg: how many parts the draw budget has left out this frame (game/draw_budget.h)
	jo_printf(0, 7, "dr%d dm%d ds%d bg%d     ", perf_part_peak[PERF_DRONE], perf_part_peak[PERF_DRONE_MODEL],
			  perf_part_peak[PERF_DRONE_SCAN], budget.dropped);
	// SG-9: how deep the wall that covers the screen is (level_screen_wall_limit: nothing further is
	// drawn), "sw-" when none does. Row 10: under the profiler's row 9
	if (level_screen_wall >= 0)
		jo_printf(0, 10, "sw%-5d", level_screen_wall);
	else
		jo_printf(0, 10, "sw-    ");
	// Where you are in the city: the page you stand on in Low Work RAM (city.h; the plan,
	// ram_banks_plan.svg), to name where a frame was slow. (foot_tower_address is the short code
	// the films' boot_at takes: row << 8 | column << 4)
	{
		const int address = foot_tower_address();

		if (address >= 0)
		{
			const unsigned int page = city_tower_address((address >> 4) & 15, address >> 8);

			jo_printf(22, 7, "at %04X_%04X ", page >> 16, page & 0xFFFF);
		}
		else
			jo_printf(22, 7, "             ");
	}
	// VDP1's own figures (game/vdp1_gauge.h): frames it didn't finish this last second, % of frames
	// it was still drawing when the CPU was done (high with a low cpu: VDP1 limits the frame
	// rate), how many commands its list had, and the chip's version
	// (over/all: this second's, then every one since boot)
	jo_printf(0, 6, "vdp1 over %d/%d busy %d%% cmd %d v%d     ", perf_vdp1.over, perf_vdp1.unfinished_total,
			  perf_vdp1.busy_percent, perf_vdp1.commands, perf_vdp1_version);
}


static void debug_flight(void)
{
	static int frames_until_refresh = 0;
	static bool shown = false;
	int frame = view_intro_frame();
	int arriving = view_arrival_frame();

	if (arriving >= 0)
		frame = arriving;
	if (frame < 0 || !settings_on(&game_settings, SETTING_FLIGHT_TEXT))
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

// What the terminal screen shows this frame, if anything: after a crash (game/game_state.h), or
// the reset flashing in during the designed crash's tear (game/breach.h)
static bool terminal_now(const char **text, int *count, bool *cursor)
{
	if (game.phase == GAME_TERMINATED)
	{
		*text = GAME_TERMINAL_TEXT;
		*count = game_typed_chars(&game);
		*cursor = game_cursor_on(&game);
		return true;
	}
	if (arena_breach.glitch.reset_shows)
	{
		*text = GAME_TERMINAL_TEXT;
		*count = breach_typed_chars(&arena_breach); // cut off part way: it never finishes
		*cursor = true;
		return true;
	}
	if (arena_breach.phase == BREACH_CODE)
	{
		*text = breach_code[0]; // the code screen's (draw_terminal types all its lines)
		*count = breach_code_typed(&arena_breach);
		*cursor = true;
		return true;
	}
	return false;
}

// The designed crash's code screen (game/breach.h): `count` characters of its lines typed so far,
// one line to a row, the cursor after the last. Printed again each frame (it only grows)
static void draw_code(int count)
{
	for (int row = 0; row < BREACH_CODE_LINES && count > 0; ++row)
	{
		const char *line = breach_code[row];
		char typed[41];
		int n = 0;

		while (line[n] != 0 && n < count && n < 40)
		{
			typed[n] = line[n];
			n++;
		}
		typed[n] = 0;
		if (n > 0)
			jo_printf_with_color(TERMINAL_X, TERMINAL_Y + row, terminal_palette.id, "%s", typed);
		count -= n;
		// The cursor after the last character typed: at this line's end if the next isn't begun
		if (count == 0 || row == BREACH_CODE_LINES - 1)
			hud_draw_text_cursor(TERMINAL_X + n, TERMINAL_Y + row);
		if (line[n] != 0)
			break; // this line's not finished: nothing typed below it yet
	}
	jo_set_printf_color_index(JO_COLOR_INDEX_White); // back to white for the debug readout
}

// Green terminal text typed out on black, behind a block cursor. Called every frame; returns
// false (drawing nothing) when the terminal isn't showing
static bool draw_terminal(void)
{
	static bool showing = false;
	static const char *shown = JO_NULL; // the text on the screen now
	const char *text;
	char typed[41]; // a whole row, and its end
	int count;
	bool cursor;

	if (!terminal_now(&text, &count, &cursor))
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
	if (!showing || text != shown)
	{
		jo_clear_screen();
		jo_set_displayed_screens(TERMINAL_SCREENS);
		showing = true;
		shown = text;
	}
	if (arena_breach.phase == BREACH_CODE && text == breach_code[0])
	{
		draw_code(count);
		return true;
	}
	// What's typed so far. The text only grows, so the screen needn't be cleared each frame
	for (int i = 0; i < count && i < 40; ++i)
		typed[i] = text[i];
	typed[count < 40 ? count : 40] = 0;
	if (count > 0)
		jo_printf_with_color(TERMINAL_X, TERMINAL_Y, terminal_palette.id, "%s", typed);
	jo_set_printf_color_index(JO_COLOR_INDEX_White); // back to white for the debug readout
	// The cursor sits on the next character's cell
	if (cursor)
		hud_draw_text_cursor(TERMINAL_X + count, TERMINAL_Y);
	return true;
}

/*
 * Hitting the wall (the designed crash, game/breach.h): the floor goes pure white, the grid's
 * navy and its lines alike, from the hit on. The floor's a paletted VDP2 image, so it's its
 * palette that changes: every one of its colours in VDP2's colour RAM written white, and back as
 * loaded when the game starts again. The sky has its own palette, so it's untouched (a colour
 * offset would have whitened the sky too: they're the same screen, RBG0). The far floor still
 * fades to black (floor_fade_vdp2.c). 256 writes, only on the frame it changes
 */
#define FLOOR_AS_LOADED 0
#define FLOOR_WHITE 1
/*
 * On foot (the corridor and the city), black: the floor's not a floor there. The VDP2 floor is one
 * plane at one height, and on foot there's no height that fits both the corridor's floor (0) and
 * the city's ground (3000 down). Moved down under the city, its grid lines shimmered between navy
 * and white as you turned (too fine to see that far off) and showed through the glass's seams as
 * a black band. So on foot it's the background only, like the sky: everything you stand on and
 * see is VDP1's (level.c), as the 3D Saturn games that look stable do it
 */
#define FLOOR_BLACK 2
/*
 * ...but out in the city (level_in_city), the circuit board the towers stand on (game/board.h):
 * the same tile as the grid, the palette swapped for the board's (FLOOR.TGA holds both), drawn 32
 * times larger (view.c, BOARD_FLOOR_SHIFT) at the towers' feet. Its pulses move on a phase every
 * BOARD_STEP_FRAMES frames: the board's 180 colours written again then, ~1% of a frame's bus
 * time at most, and nothing redrawn. Without it, what's under the skyline was black: the city
 * floated
 */
#define FLOOR_BOARD 3
// What the board fades into far off: black, as the fog everywhere else. It was a deep
// blue-violet haze (10, 12, 34) at first, so below the skyline read as distance; the user wanted
// the black back (2026-11-13: "make the fog on foot black again, not this purple"). Kept as its
// own colour, so a tint is one line if it's wanted again
#define BOARD_HAZE JO_COLOR_Black

static void set_floor_colours(int colours)
{
	static int now = FLOOR_AS_LOADED;
	static unsigned int board_frames = 0;

	if (floor_pal.data == JO_NULL)
		return;
	if (colours == FLOOR_BOARD)
	{
		// The pulse's step: a phase on every BOARD_STEP_FRAMES (a power of two: a shift). Written on
		// the first frame of each step, and on the frame the board comes in
		const unsigned int step = board_frames >> BOARD_STEP_SHIFT;
		const bool new_step = (board_frames & (BOARD_STEP_FRAMES - 1)) == 0;

		++board_frames;
		if (now != FLOOR_BOARD || new_step)
			board_palette(floor_pal.data, step);
		now = colours;
		return;
	}
	if (colours == now)
		return;
	now = colours;
	for (int i = 0; i < FLOOR_COLOURS; ++i)
		floor_pal.data[i] = colours == FLOOR_WHITE	 ? JO_COLOR_RGB(255, 255, 255)
							: colours == FLOOR_BLACK ? JO_COLOR_RGB(0, 0, 0)
													 : floor_colours[i];
}

/*
 * The designed crash's glitches (game/breach.h), all hardware tricks that cost next to nothing:
 *   - the colour offset: VDP2 adds a tint to every pixel on screen (as the prologue's flash does)
 *   - the 3D picture's jump: SGL's projection centre moved (slWindow, via jo_3d_window): every
 *     polygon lands that far off on screen. The floor's set by VDP2 from the camera, not by
 *     the polygons, so the walls and trails slip off it
 * The tint's set every frame it's on (and turned off once after): before the terminal can take
 * the screen, so a tint is gone again by the next frame whatever's drawn
 */
static void apply_breach_glitch(void)
{
	static bool tinting = false;
	static int shift_x = 0, shift_y = 0;
	const breach_glitch *g = &arena_breach.glitch;

	if (g->red | g->green | g->blue)
	{
		jo_set_screen_color_filter_a(JO_ALL_SCROLL_SCREEN, g->red, g->green, g->blue);
		tinting = true;
	}
	else if (tinting)
	{
		jo_disable_screen_color_filter(JO_ALL_SCROLL_SCREEN);
		tinting = false;
	}
	if (g->shift_x != shift_x || g->shift_y != shift_y)
	{
		shift_x = g->shift_x;
		shift_y = g->shift_y;
		jo_3d_window(0, 0, JO_TV_WIDTH - 1, JO_TV_HEIGHT - 1, draw_distance, JO_TV_WIDTH_2 + shift_x,
					 JO_TV_HEIGHT_2 + shift_y);
	}
}

// The tear's bands, over everything (VDP1 quads, the whole width of the screen)
static void draw_breach_bands(void)
{
	const breach_glitch *g = &arena_breach.glitch;

	for (int i = 0; i < g->band_count; ++i)
		hud_draw_band(g->bands[i].top, g->bands[i].height, g->bands[i].pink != 0);
}

// The status line during play (game/status_message.h): centred above the horizon, in the
// terminal's green behind the same block cursor, so it reads as the same machine talking.
// The text layer keeps what's printed, so it's only printed when another character is typed,
// and blanked once when the message goes
#define STATUS_ROW 13 // text rows are 8 pixels: 104 down, on the dark sky above the horizon (row 17,
					  // just above the radar, sat on the horizon's pink glow: green on pink was hard to read)
#define STATUS_BLANK "                                        " // a whole row: 40 characters

/*
 * The pause menu (START; game/settings.h), on the text layer below the status line, in the
 * terminal's green:
 *                 PAUSED
 *       > Debug text .......... on
 *         Flight text ......... on
 *         ...
 *        L next   R change   START play
 * The text layer keeps what's printed, so it's printed only when it's opened or a switch or the
 * cursor moves (shown_as: what's on screen), and blanked once when it closes
 */
#define PAUSE_ROW 15 // the title; the switches below it, then the buttons' line
#define PAUSE_COLUMN ((40 - SETTINGS_LINE_LENGTH) >> 1)

/*
 * The benchmark (BENCH, game/bench.h): while it runs, which view it's at, on the bottom row; once
 * it's done, the whole screen green or red with a line a view under the debug text:
 *     0650 back   late 141 over   0 pk 123 FAIL
 *     hub q2      late   0 over   0 pk  74 ok
 * "known" in place of FAIL: it fails today, and has a backlog row (it doesn't turn the screen red).
 * tests/e2e/run.sh's wait_verdict reads the colour; the shot keeps the lines
 */
#define BENCH_ROW 10 // the verdict's first line: under the debug text's rows
#define BENCH_FRC_PER_FIELD 13982 // FRC counts in a field (utils/perf.c, NTSC): VDP1's time as a %

static void draw_bench(void)
{
	const bench_run *b = input_bench();

	if (b->view < bench_view_count)
	{
		jo_printf(0, 27, "bench %d/%d %s            ", b->view + 1, bench_view_count, bench_views[b->view].name);
		return;
	}
	hud_draw_verdict(bench_passed(b));
	// L late frames, O frames VDP1 didn't finish, C the worst CPU, V VDP1's longest drawing (both
	// % of a field; V only with jo's profiler built in, else 0)
	jo_printf(0, BENCH_ROW, "BENCH %s   L late O over C cpu V vdp1   ", bench_passed(b) ? "PASS" : "FAIL");
	for (int i = 0; i < bench_view_count; ++i)
	{
		const bench_result *r = &b->results[i];

		// (36 characters: the right edge of the 40 columns is under the TV's border)
		// (a division a line, once the run's over)
		jo_printf(0, BENCH_ROW + 1 + i, "%-10s L%3d O%3d C%3d V%3d %s  ", bench_views[i].name, r->late, r->over,
				  r->peak, r->vdp1_peak * 100 / BENCH_FRC_PER_FIELD, r->pass ? "ok" : bench_views[i].known ? "known" : "FAIL");
	}
	jo_printf(0, 27, "                                       ");
}

static void draw_pause_menu(bool paused)
{
	static bool shown = false;
	static settings shown_as;
	char line[SETTINGS_LINE_LENGTH + 1];

	if (!paused)
	{
		if (shown)
			for (int row = 0; row < SETTING_COUNT + 3; ++row)
				jo_printf(0, PAUSE_ROW + row, STATUS_BLANK);
		shown = false;
		return;
	}
	// A dark box behind it, every frame (VDP1 draws its list afresh each frame; the text stays)
	hud_draw_panel(PAUSE_COLUMN - 2, PAUSE_ROW, 30, SETTING_COUNT + 3);
	if (shown)
	{
		bool same = shown_as.cursor == game_settings.cursor;

		for (int which = 0; which < SETTING_COUNT; ++which)
			same = same && shown_as.value[which] == game_settings.value[which];
		if (same)
			return; // nothing's changed: what's on screen is still right
	}
	shown = true;
	shown_as = game_settings;
	jo_printf_with_color(PAUSE_COLUMN + 10, PAUSE_ROW, terminal_palette.id, "PAUSED");
	for (int which = 0; which < SETTING_COUNT; ++which)
	{
		settings_line(&game_settings, (setting)which, line);
		jo_printf_with_color(PAUSE_COLUMN, PAUSE_ROW + 1 + which, terminal_palette.id, "%s", line);
	}
	jo_printf_with_color(PAUSE_COLUMN - 2, PAUSE_ROW + 2 + SETTING_COUNT, terminal_palette.id,
						 "L next   R change   START play");
	jo_set_printf_color_index(JO_COLOR_INDEX_White); // back to white for the debug readout
}

// hidden: the HUD's off (the prologue, the flight down): any text left on the row is wiped. It's
// printed on VDP2's text layer, which keeps it until it's written over, so leaving play for the
// prologue (the debug camera's A) left the last message showing
static void draw_status(bool hidden)
{
	static int printed = 0; // characters on screen now
	int count = status_typed_chars(&status);
	int column = (40 - status.length) >> 1; // centred on the 40 character row

	if (hidden || !status_visible(&status))
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


/*
 * Where a point is on screen, in pixels from the middle (+y down), as SGL's slConvert3Dto2D
 * did: the point through the current matrix (the camera's and the world's) into the view, then
 * the perspective, x * distance / z. Two divisions, by the SH-2's divider (jo_fixed_div): once
 * a frame. A point behind the camera (or at it) gives the middle
 */
static void screen_point(const jo_pos3D_fixed *world, int *x, int *y)
{
	jo_pos3D_fixed view;

	jo_3d_transform_point(world, &view);
	*x = 0;
	*y = 0;
	if (view.z <= 0)
		return;
	*x = jo_fixed2int(jo_fixed_mult(view.x, jo_fixed_div(jo_3d_get_projection_distance(), view.z)));
	*y = jo_fixed2int(jo_fixed_mult(view.y, jo_fixed_div(jo_3d_get_projection_distance(), view.z)));
}

/*
 * The screen's red glow while a watchdog's beam has you (game/seen_glow.h): VDP2's colour offset,
 * as the breach's tint, so nothing more is drawn. The breach is in the arena and the glow on foot,
 * so the two never want the offset at once. Turned off once when it's faded
 */
static void apply_seen_glow(void)
{
	static bool glowing = false;
	int red, green, blue;

	seen_glow_offset(&watchdog_glow, &red, &green, &blue);
	if (view_is_on_foot() && (red | green | blue))
	{
		jo_set_screen_color_filter_a(JO_ALL_SCROLL_SCREEN, red, green, blue);
		glowing = true;
	}
	else if (glowing)
	{
		jo_disable_screen_color_filter(JO_ALL_SCROLL_SCREEN);
		glowing = false;
	}
}

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
	// perf_frame_start (an earlier callback) has just read whether VDP1 finished the frame before
	if (DRAW_BUDGET)
		draw_budget_frame(&budget, perf_vdp1.last_unfinished);
	/*
	 * The pause menu's "Wireframe" (the user, 2026-11-18: "can that be togglable from debug menu?"):
	 * jo draws every polygon as its outline, in green (jo_3d_set_wireframe; jo turns the frame's
	 * commands into polylines as it sends them). So what's drawn behind what shows, live: the
	 * layers LEVELS.md checks for, without a build of its own. Read once a frame, a flag: free
	 */
	jo_3d_set_wireframe(settings_on(&game_settings, SETTING_WIREFRAME));
	apply_breach_glitch();
	apply_seen_glow();
	set_floor_colours(view_is_on_foot()					 ? (level_in_city() ? FLOOR_BOARD : FLOOR_BLACK)
					  : arena_breach.phase != BREACH_NONE ? FLOOR_WHITE
														  : FLOOR_AS_LOADED);
	// The far floor fades to BOARD_HAZE over the board (black now), black anywhere else: a black
	// board fogged in a tint would be a tinted floor in the corridor (floor_fade_vdp2.h)
	floor_fade_set_colour(view_is_on_foot() && level_in_city() ? BOARD_HAZE : JO_COLOR_Black);
	// Terminated (or the designed crash's reset, outside): only the terminal text. The world
	// isn't drawn at all
	if (draw_terminal())
	{
		perf_frame_end();
		return;
	}

	// SKY: scrolls with the heading, outside the world matrix. Before the camera's matrix: jo
	// starts each frame with none (the identity), as in jo's own plane sample. After it, on
	// jo-engine 2026 the sky came out wholly transparent: the camera looks along -z, and jo takes
	// the sky's depth from that matrix (jo's vdp2.c, jo_vdp2_set_rotation_matrix)
	// VDP2's floor and sky are drawn with the planes' camera (view.h, view_vdp2_frame): this
	// frame's, now that jo shows them with this frame's VDP1 picture (johannes-fetz/joengine#111)
	view_vdp2_frame();
	if (SHOW_SKY)
	{
		jo_3d_push_matrix();
		{
			// jo_angle / 104 ~= radians * 100. Up and down with the camera's pitch (view_sky_rise):
			// looking up on foot, the sky stayed put while the world went down the screen, and a
			// band opened between the sky's horizon and the floor's
			jo_3d_translate_matrix(view_vdp2()->rz / 104, -2 - view_vdp2()->sky_rise, 100);
			jo_background_3d_plane_b_draw(false);
		}
		jo_3d_pop_matrix();
	}

	jo_3d_camera_look_at(&cam);

	//HUD
	//draw_hud();

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

	//  FLOOR: VDP2's, so with the planes' camera (view_vdp2_frame), in a matrix of its own
	{
		const view_vdp2_camera *v = view_vdp2();
		const int grid_frc = jo_time_get_frc(); // the debug line's "gr" (utils/perf.h): the grid

		view_push_vdp2_matrix();
		jo_3d_push_matrix();
		{
			// The world's matrix (view.c) is the camera's turn, the world at 1/VIEW_WORLD_SCALE, and
			// the camera's position. The floor undoes the last two and does them again its own way
			// (FLOOR_VIEW_SCALE): the position back, the world 17/16 its size round the camera
			// (the picture doesn't change: VIEW_WORLD_SCALE's own trick), then the camera's place
			// within the grid's repeat (floor_wrap). On foot, far down: the ground under Work
			// RAM's city (view.h)
			jo_3d_translate_matrix(v->x, v->y, -v->z);
			// (Drawn larger, the floor's texels are bigger: the world's shrunk by as much, and the
			// texels a matrix unit stay the same. FLOOR_VIEW_SCALE says why)
			matrix_scale(FLOOR_VIEW_SCALE >> view_floor_shift, FLOOR_VIEW_SCALE >> view_floor_shift,
						 FLOOR_VIEW_SCALE >> view_floor_shift);
			jo_3d_translate_matrix(-floor_wrap(v->x), -floor_wrap(v->y), v->z + v->ground);
			// Then the floor's texels in world units. Spectating, it's drawn larger (view_floor_shift)
			matrix_scale(FLOOR_SCALE << view_floor_shift, FLOOR_SCALE << view_floor_shift, JO_FIXED_1);
			jo_background_3d_plane_a_draw(true);
		}
		jo_3d_pop_matrix();
		jo_3d_pop_matrix();
		perf_part_add(PERF_GRID, grid_frc);
	}

	view_push_world_matrix();
	// The slave SH-2 draws the corridor's floor and boxes from the world's matrix while this CPU
	// goes on (src/slave_draw.h): handed over as they're built, waited for before the pop below
	slave_draw_begin();
	{
		int floor_frc;

		floor_frc = jo_time_get_frc(); // and "fl": the corridor's glass floor

		// The corridor behind the flicker, once it's given way: its glass floor over the grid
		if (!spectating)
		{
			level_view_begin(arena_corridor, debug_camera); // the planes everything on foot is cut to
			level_screen_wall_limit(); // SG-9: before anything's drawn (the slave starts at once)
			// The roofs the nearest watchdog's scan lights, coloured as the city's drawn (drone_draw.h)
#if CITY_WATCHDOG
			if (view_is_on_foot() && level_in_city())
				drone_draw_prepare(watchdogs, CITY_WATCHDOGS, watchdog_boxes, CITY_TOWERS);
			else
#endif
				level_scan_set(JO_NULL, 0, 0);
			draw_corridor_floor(arena_corridor, clip);
			perf_part_add(PERF_FLOOR, floor_frc);
			floor_frc = jo_time_get_frc();
			draw_corridor_boxes(arena_corridor, clip);
			perf_part_add(PERF_TOPS, floor_frc);
			perf_px_mark(PX_WORLD); // (the slave's share comes in as PX_SLAVE, utils/perf.h)
		}

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
			// Always: on foot in the city this is the pyramid, a landmark, and the draw budget leaving
			// it out lost it from the CD block's lift (game/draw_budget.h)
			draw_arena_walls(&visible_walls, &wall_eye, clip, !first_person && !debug_camera, arena_wall_rise(),
							 &arena_flicker, arena_corridor);
			// The ground round the arena, raised level with the walls' tops: the arena's sunk into
			// it, and the floor's grid is only seen inside. It rises with the walls, from the
			// break's flash (flat on the floor till then, hiding the grid outside). Between rounds,
			// once it's sunk flat, it goes: the grid runs on for ever, as in the loop
			// (Not on foot: from out there the arena's a pyramid, level.c's arena_pyramid_add)
			if (arena_rounds.phase != ROUND_OPEN && !view_is_on_foot())
				draw_arena_surround(arena_wall_rise(), !first_person && !debug_camera);
			perf_px_mark(PX_ARENA);
		}

		if (enable_trails && player1_trail_mesh != JO_NULL) // not after the arena's memory's given back
		{
			int trails_frc = jo_time_get_frc(); // the debug line's "trl" figure (utils/perf.h)
			// 1st person (the eye is below the trails' tops): a part of a trail hidden behind a
			// nearer one isn't drawn. VDP1 has no depth buffer, and side by side pieces of two
			// parallel trails sort in either order: the hidden one showed through
			const trail_occluders *hide = JO_NULL;

			if (clip)
			{
				trail_occluders_begin(&occluders, clip->eye);
				// The blocks first (the donut's, the pillars, the rooms' walls), once they're up:
				// rising or sinking, they're lower than a trail, so they may not hide it all
				if (arena_wall_rise() >= ROUND_WALLS_FULL)
				{
					int block_count;
					const world_rect *blocks = arena_blocks(&block_count);

					trail_occluders_add_blocks(&occluders, blocks, block_count, &visible_trails);
				}
				// Only trails standing full height: a sinking one is lower than the eye, hiding nothing
				for (int i = 0; i < solid_trail_count; ++i)
					trail_occluders_add(&occluders, solid_trails[i], TRAIL_HEAD_GAP, &visible_trails);
				hide = &occluders;
			}
			trail_mesh_draw(player1_trail_mesh, &player1.wall, player1.trail_height, TRAIL_HEAD_GAP, player1.bike.position,
							&visible_trails, clip, hide);
			// Cut finer near the player (the camera), not near the bots: that's where detail shows.
			// A dead bot's trail sinks (its height falls), then has nothing left to draw
			for (int i = 0; i < BOT_COUNT_MAX; ++i)
				if (bots[i].trail_height > 0)
					trail_mesh_draw(&bot_trail_meshes[i], &bots[i].wall, bots[i].trail_height, TRAIL_HEAD_GAP,
									player1.bike.position, &visible_trails, clip, hide);
			// The prologue's loop: the finished strokes of its message, each as tall as its word
			// stands (sinking once it's spelt). All of them in one mesh, drawn once (trail_render.h)
			trail_mesh_begin(attract_mesh);
			for (int i = 0; i < attract_stroke_count; ++i)
			{
				int height = attract_word_height[attract_stroke_word[i]];

				if (height > 0)
					trail_mesh_add(attract_mesh, &attract_strokes[i], height, 0, player1.bike.position,
								   &visible_trails, clip, hide);
			}
			trail_mesh_end(attract_mesh);
			perf_part_add(PERF_TRAILS, trails_frc);
			perf_px_mark(PX_TRAILS);
		}

		// In 1st person the camera is inside it; coming down into it, it's seen until the camera's close
		if (!first_person || spectating ||
			(view_arriving() && JO_ABS(view_x - player1.bike.position.x) + JO_ABS(view_y - player1.bike.position.y) > 150))
			draw_cycle(player1.bike.position, player1.bike.heading, PLAYER_COLOUR, player1.alive);
		// Where the cycle is on screen, for the burst of light round it (draw_loop_break): SGL
		// works it out through the camera, so it has to be asked in here. In 1st person the
		// camera's inside it: the middle of the screen
		burst_x = 0;
		burst_y = 0;
		if (spectating || view_arriving())
		{
			// Coming down, the last burst stays where the cycle was when the loop broke
			world_point at = spectating ? player1.bike.position : loop_break_at;
			const jo_pos3D_fixed middle = {jo_int2fixed(at.x), jo_int2fixed(at.y), jo_int2fixed(PLAYER_HALF)};
			int on_screen_x;
			int on_screen_y;

			screen_point(&middle, &on_screen_x, &on_screen_y);
			// Kept on the screen: in the opening flight the cycle can be off it (or behind the
			// camera), and a burst drawn round a point far outside it would wrap into spikes
			burst_x = JO_MAX(-JO_TV_WIDTH_2, JO_MIN(JO_TV_WIDTH_2, on_screen_x));
			burst_y = JO_MAX(-JO_TV_HEIGHT_2, JO_MIN(JO_TV_HEIGHT_2, on_screen_y));
		}
		// 1st person: only if it's wholly in front of the camera (SGL would put one behind the
		// camera on screen anyway, as if in front) and not wholly off to one side
		// Not on foot: they're back in the arena, behind its walls and the mirror over the hole,
		// and VDP1 has no depth: one drawn showed through the city's towers (seen in play, 2026-11-13)
		for (int i = 0; i < BOT_COUNT_MAX; ++i)
			if (bots[i].alive && !view_is_on_foot() &&
				(!clip || wall_clip_object_visible(clip, bots[i].bike.position, CYCLE_RADIUS, TRAIL_MIN_FORWARD)))
				draw_cycle(bots[i].bike.position, bots[i].bike.heading, bot_trail_colours[i], true);
	}
	// The landmarks on the horizon and the CD block's disc over the city (level.h): in play and
	// on foot, each from everywhere but its own place
	if (!spectating)
	{
		static unsigned int sky_frame = 0;
		int side, wall, middle;
		bool in_lift;

		// The landmarks are the first thing to go when VDP1 is short (game/draw_budget.h): on the
		// horizon, the farthest. The skyline behind the towers always stays
		// The slave's share of the city (level.c, grey_slave_job) reads level_view; the sky's ring
		// cut changes it: the slave's done first. Only when it has one (level_city_shared)
		if (level_city_shared)
			slave_draw_wait();
		if (foot_cd_hub(&side, &wall, &middle, &in_lift))
			level_sky_draw(side, wall, middle, sky_frame++, view_is_on_foot() ? PLACE_WORK_RAM : PLACE_ARENA, in_lift,
						   draw_budget_allows(&budget, BUDGET_LANDMARKS));
		perf_px_mark(PX_SKY);
		// The watchdogs' drones and their beams (drone_draw.h): out in the city only. From inside the
		// corridor its walls would hide them, and VDP1 has no depth to know that
#if CITY_WATCHDOG
		if (view_is_on_foot() && level_in_city())
			for (int i = 0; i < CITY_WATCHDOGS; ++i)
				drone_draw(&watchdogs[i], sky_frame);
#endif
		perf_px_mark(PX_DRONE);
	}
	// Your body (game/body.h), once you have one (from the crash on): posed once here, then drawn
	// from your own eyes, and again in the mirror below
	{
		static body player_body;
		static bool had_body = false;
		body_state doing;
		int yaw, feet;

		if (!spectating && foot_body_state(&doing, &yaw, &feet))
		{
			if (!had_body)
				body_init(&player_body); // a new one: its first pose is where it is, not eased to
			had_body = true;
			level_body_set(&player_body, &doing, yaw, feet);
			level_body_draw(true);
			perf_px_mark(PX_BODY);
		}
		else
		{
			had_body = false;
			level_body_clear();
		}
	}
	slave_draw_end(); // everything handed to the slave is drawn
	// SG-9's depth limit (level_screen_wall_limit) was the world's, this frame's: none for the mirror
	// (its reflection is deeper than its glass) or the HUD (jo tests sprites against it too)
	jo_3d_set_depth_limit(-1);
	// The mirror over the hole you broke through (level.h): the corridor again, reflected in it.
	// After the slave's done with the meshes: the second pass fills them again
	{
		wall_clip_camera mirrored;

		if (!spectating && clip != JO_NULL && level_mirror_begin(arena_corridor, clip, &mirrored))
		{
			const int mirror_frc = jo_time_get_frc(); // the debug line's "mi" (utils/perf.h)

			jo_3d_push_matrix();
			level_mirror_matrix();
			slave_draw_begin();
			// You, in the glass (the same joints your own eyes see): first, so the slave works it out
			// while this CPU does the reflected corridor (level.h)
			level_body_draw(false);
			level_mirror_draw(arena_corridor, &mirrored);
			slave_draw_end();
			jo_3d_pop_matrix();
			level_mirror_end(arena_corridor, clip);
			perf_part_add(PERF_MIRROR, mirror_frc);
			perf_px_mark(PX_MIRROR);
		}
	}
	jo_3d_pop_matrix();

	draw_loop_break();
	// Spectating, you're only watching: no HUD (the gauge, the radar and the status line), nor
	// while the camera comes down into the cycle: it appears as you take over
	draw_pause_menu(debug_camera && pause_menu_up);
	if (spectating || view_arriving())
	{
		draw_status(true);
		debug_3d(false);
		debug_flight();
		perf_frame_end();
		return;
	}
	// On foot there's no map: the user asked for it gone ("remove the HUD map when on foot"); the
	// arena's trails are no guide in the city, and the screen is yours to look about. Nor the boost
	// gauge: it's the map's frame, and there's no boost on foot
	if (!view_is_on_foot())
		hud_draw_boost(&player1.bike); // on top of the world: screen pixels, no perspective
	if (!debug_camera && !view_is_on_foot()) // both views: in iso it turns with the iso camera (hud.h)
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
	{
		int x, y;

		if (!debug_camera && foot_target_on_screen(&x, &y))
			hud_draw_lock(x, y); // the head's lock (game/target.h)
	}
	draw_status(false);
	draw_breach_bands();
	debug_3d(settings_on(&game_settings, SETTING_DEBUG_TEXT));
	if (BENCH)
		draw_bench();
	// debug_pad1();
	perf_frame_end(); // includes the debug text
}

/*
 * A cycle isn't a vehicle: it's a CPU process (docs/STORY.md), and it's what writes its trail. So
 * it's a signal: a ribbon of light that leads with a horizontal edge as wide as the cycle (what
 * hits a wall is what you see) and twists a quarter turn along its length into the upright edge
 * its trail starts from. The leading edge rises and falls as it rides, a wave: it goes by how far
 * the cycle's ridden, so it's quicker the faster you go, still when you stop, and level, a flat
 * line, once the process is terminated. (A white square pulse stepping along the edge was tried
 * first: it read as something stuck on. The cube before it was a placeholder; other shapes are in
 * docs/BACKLOG.md.)
 *
 * The shape, in its own space (the cycle's place on the floor at the origin, facing -y, as heading
 * north): five cross-sections from the front (y = -PLAYER_HALF) to the back (+PLAYER_HALF), each a
 * line through (0, y, TRAIL_HEIGHT / 2), turned 0, 22.5, 45, 67.5 and 90 degrees from level and a
 * little longer each time (the width at the front, the trail's height at the back). Four strips
 * join them.
 *
 * One model, built once here. Each rider is the same points moved and turned by the matrix
 * (draw_cycle): nothing's built per rider or per frame. Only what differs is written before a
 * draw: the five colours, and how high the front two cross-sections are (the wave)
 */
#define RIBBON_MIDDLE (TRAIL_HEIGHT / 2)
// The wave: the leading edge's height above (or below) the middle, a sine of 16 steps, one step
// for every 16 units ridden: a whole rise and fall every 256, about 1.5 a second at cruise and 3
// at a full boost (every 64 was a flutter, every 128 still looked funny: 2026-11-11). +-12 of
// the trail's 60. The next cross-section back moves half as far, so the twist stays smooth, and
// the rest stay put: the back edge must meet the trail
#define WAVE_STEPS 16
#define WAVE_STEP_SHIFT 4
#define RIBBON_SECOND_UP 9 // the second cross-section's ends: sin(22.5 degrees) x its half length, 26
static const signed char wave[WAVE_STEPS] = {0, 5, 8, 11, 12, 11, 8, 5, 0, -5, -8, -11, -12, -11, -8, -5};

/*
 * One flat, unlit polygon of a cycle's model: its four corners (a three-cornered face names its
 * last corner twice), what VDP1 draws it as (JO_VDP1_POLYGON, JO_VDP1_LINE) and whether it's seen
 * from both sides (JO_3D_DUAL_PLANE: a surface, the ribbon) or only from outside
 * (JO_3D_SINGLE_PLANE: a solid's face. jo tells the outside by the corners going clockwise on
 * screen, and skips the faces turned away: half a solid's polygons never reach VDP1)
 */
static void cycle_polygon(jo_3d_polygon *q, jo_3d_polygon_attribute *a, int p0, int p1, int p2, int p3, unsigned int kind,
						  unsigned char plane)
{
	q->normal.x = JO_FIXED_0; // unlit
	q->normal.y = JO_FIXED_0;
	q->normal.z = JO_FIXED_1;
	q->vertices[0] = (unsigned short)p0;
	q->vertices[1] = (unsigned short)p1;
	q->vertices[2] = (unsigned short)p2;
	q->vertices[3] = (unsigned short)p3;
	a->plane = plane;
	a->sort = JO_3D_SORT_CENTER | ((kind >> 16) & 0x1c) | JO_3D_NO_OPTION;
	a->texture_id = JO_3D_NO_TEXTURE;
	a->draw_mode = (unsigned short)((JO_VDP1_COLOR_MODE_RGB | JO_VDP1_NO_GOURAUD) | ((kind >> 24) & 0xc0));
	a->color = JO_COLOR_White;
	a->gouraud_table = 0;
	a->direction = (unsigned short)(kind & 0x3f);
}

static void cycle_point(jo_pos3D_fixed *p, int x, int y, int z)
{
	p->x = jo_int2fixed(x);
	p->y = jo_int2fixed(y);
	p->z = jo_int2fixed(z);
}

/*
 * The token: a process as a packet of data, a diamond standing on its point. Two four-sided
 * pyramids base to base: as wide as the cycle round the middle (what hits a wall), as tall as the
 * trail, which is its upright axis left behind. It spins about that axis as it rides (draw_cycle:
 * the matrix does it, nothing's written), one face white, so a flash comes round each turn. And it
 * looks the same after a quarter turn, so turning 90 degrees shows no snap.
 * In its own space: the foot at the origin, the four middle corners out along x and y.
 * It leans forwards as it rides (TOKEN_LEAN, about the foot): going somewhere
 */
#define TOKEN_LEAN 0x0e00 // about 20 degrees (0x10000 a whole turn)
// Where that lean puts the token's middle (CYCLE_LINKED's ribbon starts there): ahead of the
// cycle's place, and up. And the ribbon's length from there back to the trail's start
#define LINK_FRONT 10
#define LINK_MIDDLE 28
#define LINK_LENGTH (LINK_FRONT + PLAYER_HALF + 1) // 36: divides by 4
static void load_token(void)
{
	static const signed char corner[4][2] = {{PLAYER_HALF, 0}, {0, PLAYER_HALF}, {-PLAYER_HALF, 0}, {0, -PLAYER_HALF}};

	cycle_point(&token_points[0], 0, 0, 0);
	for (int k = 0; k < 4; ++k)
		cycle_point(&token_points[1 + k], corner[k][0], corner[k][1], TRAIL_HEIGHT / 2);
	cycle_point(&token_points[5], 0, 0, TRAIL_HEIGHT);
	for (int k = 0; k < 4; ++k)
	{
		const int here = 1 + k, next = 1 + ((k + 1) & 3);

		// The upper face and the lower one between the same two middle corners, each wound so its
		// outside is clockwise on screen
		cycle_polygon(&token_polygons[k], &token_attributes[k], 5, here, next, next, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE);
		cycle_polygon(&token_polygons[4 + k], &token_attributes[4 + k], 0, next, here, here, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE);
	}
	token.data.points = token_points;
	token.data.polygons = token_polygons;
	token.data.attributes = token_attributes;
	token.data.point_count = TOKEN_POINTS;
	token.data.polygon_count = TOKEN_FACES;
}

/*
 * The cursor: a terminal's block cursor, the thing that writes. The trail is the line already
 * written, and the block at its head blinks white (draw_cycle), by distance ridden. A slab as wide
 * as the cycle and as tall as the trail, CURSOR_DEEP thick, sheared forwards: its top CURSOR_LEAN
 * ahead of its foot (about 20 degrees), italic, in a hurry. The lean is in the points, so it costs
 * nothing. No underside: nobody's below the floor
 */
#define CURSOR_DEEP 16
#define CURSOR_LEAN 22
static void load_cursor(void)
{
	// Back and front, at the foot and at the top; left (-x) then right of each
	for (int k = 0; k < 4; ++k)
	{
		const int y = (k & 1 ? -CURSOR_DEEP / 2 : CURSOR_DEEP / 2) - (k & 2 ? CURSOR_LEAN : 0);
		const int z = k & 2 ? TRAIL_HEIGHT : 0;

		cycle_point(&cursor_points[k * 2], -PLAYER_HALF, y, z);
		cycle_point(&cursor_points[k * 2 + 1], PLAYER_HALF, y, z);
	}
	// 0 1: back foot, 2 3: front foot, 4 5: back top, 6 7: front top
	cycle_polygon(&cursor_polygons[0], &cursor_attributes[0], 2, 3, 7, 6, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE); // front
	cycle_polygon(&cursor_polygons[1], &cursor_attributes[1], 1, 0, 4, 5, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE); // back
	cycle_polygon(&cursor_polygons[2], &cursor_attributes[2], 6, 7, 5, 4, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE); // top
	cycle_polygon(&cursor_polygons[3], &cursor_attributes[3], 0, 2, 6, 4, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE); // left
	cycle_polygon(&cursor_polygons[4], &cursor_attributes[4], 3, 1, 5, 7, JO_VDP1_POLYGON, JO_3D_SINGLE_PLANE); // right
	cursor_block.data.points = cursor_points;
	cursor_block.data.polygons = cursor_polygons;
	cursor_block.data.attributes = cursor_attributes;
	cursor_block.data.point_count = CURSOR_POINTS;
	cursor_block.data.polygon_count = CURSOR_FACES;
}

void load_player_model(void)
{
	/*
	 * CYCLE_LINKED: the token alone looked detached from its trail, so a ribbon joins them. It
	 * doesn't move (no wave). The token leans forwards about its foot, so its middle isn't over
	 * the cycle's place: it's LINK_FRONT ahead and a little lower (30 up the axis, leant 20
	 * degrees: 30 sin = 10 ahead, 30 cos = 28 up). The ribbon starts there, level, inside the
	 * token (narrower than the token's middle, so it comes out of it whichever way that's spun),
	 * and twists and widens back to the upright edge the trail starts from
	 */
	const bool linked = cycle_shape() == CYCLE_LINKED;
	// Each cross-section's turn from level: cos and sin x1024 (0, 22.5, 45, 67.5, 90 degrees), and
	// half its length: half the cycle's width at the front, half the trail's height at the back
	static const short turn[RIBBON_PARTS + 1][2] = {{1024, 0}, {946, 392}, {724, 724}, {392, 946}, {0, 1024}};
	static const signed char half[RIBBON_PARTS + 1] = {PLAYER_HALF, 26, 28, 29, TRAIL_HEIGHT / 2};
	static const signed char half_linked[RIBBON_PARTS + 1] = {16, 20, 25, 28, TRAIL_HEIGHT / 2};

	if (cycle_shape() == CYCLE_TOKEN || linked)
		load_token();
	if (cycle_shape() == CYCLE_TOKEN)
		return;
	if (cycle_shape() == CYCLE_CURSOR)
	{
		load_cursor();
		return;
	}
	for (int k = 0; k <= RIBBON_PARTS; ++k)
	{
		// From the front to the back, in quarters (RIBBON_PARTS is 4: a shift). Alone: the whole
		// cycle's length. Linked: from the token's middle (LINK_FRONT ahead), 36 long
		const int y = linked ? -LINK_FRONT + ((LINK_LENGTH * k) >> 2) : -PLAYER_HALF + ((2 * PLAYER_HALF * k) >> 2);
		const int size = linked ? half_linked[k] : half[k];
		// Linked: rising the 2 from the token's middle to the trail's as it goes back
		const int middle = linked ? LINK_MIDDLE + (k >> 1) : RIBBON_MIDDLE;
		const int across = (turn[k][0] * size) >> 10, up = (turn[k][1] * size) >> 10;

		for (int end = 0; end < 2; ++end)
		{
			const int sign = end == 0 ? -1 : 1;

			cycle_point(&ribbon_points[k * 2 + end], sign * across, y, middle + sign * up);
		}
	}
	for (int k = 0; k < RIBBON_PARTS; ++k)
		cycle_polygon(&ribbon_polygons[k], &ribbon_attributes[k], k * 2, k * 2 + 1, k * 2 + 3, k * 2 + 2, JO_VDP1_POLYGON, JO_3D_DUAL_PLANE);
	// The leading edge: a line of light along the front (a VDP1 line: its first two corners)
	cycle_polygon(&ribbon_polygons[RIBBON_EDGE], &ribbon_attributes[RIBBON_EDGE], 0, 1, 1, 0, JO_VDP1_LINE, JO_3D_DUAL_PLANE);
	ribbon.data.points = ribbon_points;
	ribbon.data.polygons = ribbon_polygons;
	ribbon.data.attributes = ribbon_attributes;
	ribbon.data.point_count = RIBBON_POINTS;
	// Linked: no line of light along the front (it's inside the token): just the four strips
	ribbon.data.polygon_count = linked ? RIBBON_PARTS : RIBBON_PARTS + 1;
}

// A rider's colour, a share of the way to white (0 .. 4 quarters): per channel, RGB555, no division
static jo_color lighter(jo_color colour, int quarters)
{
	const int r = colour & 31, g = (colour >> 5) & 31, b = (colour >> 10) & 31;

	return (jo_color)(0x8000 | ((b + (((31 - b) * quarters) >> 2)) << 10) | ((g + (((31 - g) * quarters) >> 2)) << 5) |
					  (r + (((31 - r) * quarters) >> 2)));
}

/*
 * A cycle at position, facing heading (HEADING_NORTH ...), in its rider's colour (its trail's).
 * The model's turned and moved by the matrix; what's written here is only the colours (the front
 * strip nearly white, the back one the trail's own colour, so the ribbon runs into its trail) and
 * the wave: how high the leading edge is, by the position along the way it's going (no counter to
 * keep). running: false for a terminated process: a flat line, level
 */
void draw_cycle(world_point position, int heading, jo_color colour, bool running)
{
	const int along = heading == HEADING_NORTH || heading == HEADING_SOUTH ? position.y : position.x;
	const int lift = running ? wave[(along >> WAVE_STEP_SHIFT) & (WAVE_STEPS - 1)] : 0;
	// How far it's ridden, for the token's spin and the cursor's blink: x + y, which (unlike along)
	// doesn't jump at a turn. It runs backwards heading two of the four ways: the spin's the other
	// way round there, and nobody can tell
	const int ridden = position.x + position.y;

	if (cycle_shape() == CYCLE_TOKEN || cycle_shape() == CYCLE_LINKED)
	{
		// The faces: a light and a deeper shade turn about, one upper face white (the flash that
		// comes round). A terminated process has no flash (and doesn't spin: it isn't moving)
		for (int k = 0; k < 4; ++k)
		{
			token_attributes[k].color = lighter(colour, k & 1 ? 1 : 2);
			token_attributes[4 + k].color = lighter(colour, k & 1 ? 0 : 1);
		}
		if (running)
			token_attributes[0].color = JO_COLOR_White;
		jo_3d_push_matrix();
		{
			jo_3d_translate_matrix(position.x, position.y, 0);
			angle_rotate_z((jo_angle)(heading * RIBBON_QUARTER_TURN));
			if (cycle_shape() == CYCLE_LINKED)
			{
				// The ribbon from the token's middle back to the trail: turned with the cycle,
				// but not leant or spun. Lightest inside the token, the trail's own colour behind
				for (int k = 0; k < RIBBON_PARTS; ++k)
					ribbon_attributes[k].color = lighter(colour, 2 - k < 0 ? 0 : 2 - k);
				jo_3d_mesh_draw(&ribbon);
			}
			angle_rotate_x((jo_angle)TOKEN_LEAN); // forwards, about its foot
			// A whole turn every 256 units ridden (a shift: 256 x 256 is the angle's 0x10000):
			// about 1.5 a second at cruise
			angle_rotate_z((jo_angle)(ridden << 8));
			jo_3d_mesh_draw(&token);
		}
		jo_3d_pop_matrix();
		return;
	}
	if (cycle_shape() == CYCLE_CURSOR)
	{
		// The blink: white for 128 units ridden, the rider's colour for the next 128 (about 3 a
		// second at cruise). The sides and top a shade apart so it reads as a block. Terminated:
		// its colour, steady
		const int lit = running && ((ridden >> 7) & 1) ? 2 : 0;

		cursor_attributes[0].color = cursor_attributes[1].color = lit ? JO_COLOR_White : lighter(colour, 1);
		cursor_attributes[2].color = lighter(colour, 2 + lit);
		cursor_attributes[3].color = cursor_attributes[4].color = lighter(colour, lit);
		jo_3d_push_matrix();
		{
			jo_3d_translate_matrix(position.x, position.y, 0);
			angle_rotate_z((jo_angle)(heading * RIBBON_QUARTER_TURN));
			jo_3d_mesh_draw(&cursor_block);
		}
		jo_3d_pop_matrix();
		return;
	}
	for (int k = 0; k < RIBBON_PARTS; ++k)
		ribbon_attributes[k].color = lighter(colour, 2 - k < 0 ? 0 : 2 - k);
	ribbon_attributes[RIBBON_EDGE].color = JO_COLOR_White;
	// The front cross-section (level: both ends at the middle's height) up or down by the wave
	ribbon_points[0].z = ribbon_points[1].z = jo_int2fixed(RIBBON_MIDDLE + lift);
	// The next one back (turned 22.5 degrees: its ends 10 below and above the middle) by half
	ribbon_points[2].z = jo_int2fixed(RIBBON_MIDDLE - RIBBON_SECOND_UP + (lift >> 1));
	ribbon_points[3].z = jo_int2fixed(RIBBON_MIDDLE + RIBBON_SECOND_UP + (lift >> 1));
	jo_3d_push_matrix();
	{
		jo_3d_translate_matrix(position.x, position.y, 0);
		// The model faces north; a quarter turn for each heading on from it
		angle_rotate_z((jo_angle)(heading * RIBBON_QUARTER_TURN));
		jo_3d_mesh_draw(&ribbon);
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
	floor_pal = image_pal;
	for (int i = 0; i < FLOOR_COLOURS; ++i)
		floor_colours[i] = floor_pal.data[i];

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
	plane1_palette_id = image_pal.id; // my_tga_palette_handling made it a palette of its own
	hud1_texture_id = jo_sprite_add_tga(JO_ROOT_DIR, "HUD.TGA", JO_COLOR_Black);		   // spirit 7
}

void jo_main(void)
{
	// The back colour: black, or magenta in a glitch-check build (features.h GLITCH_CHECK), where a
	// hole in the level is then easy to find in a film (tests/e2e/glitch.py)
	jo_core_init(GLITCH_CHECK ? JO_COLOR_RGB(255, 0, 255) : JO_COLOR_Black);
	// Low Work RAM (1 MB at 0x00200000, the "slow" bank): jo-engine 2026 doesn't use it. As a second
	// zone for jo_malloc it's only used once the first (in high Work RAM, makefile) is full, so the
	// heap can't run short now that zone's smaller. (SCU DMA can't reach it, so data DMA'd to the
	// VDPs is better kept in high RAM: it is, while the first zone has room)
#ifdef JO_TEST_PCPROF_FRAME
	// (a profiling build: jo's PC sampler keeps its histograms in the top 128 KB, 0x002E0000 on)
	jo_add_memory_zone((unsigned char *)0x00200000, 0xE0000);
#else
	jo_add_memory_zone((unsigned char *)0x00200000, 0x100000);
#endif
	// VDP1's pixels by part on the debug line (utils/perf.h): only a profiler build counts them
	jo_profile_count_pixels(true);
	jo_set_tga_palette_handling(my_tga_palette_handling);
	// The pause menu's switches as features.h starts them: first, as the cycle's model needs its shape
	settings_init(&game_settings);

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
	drone_draw_init(); // the watchdog's drone's model
	arena_trail_meshes_take(); // after the fog's on: the meshes set their quads up for it

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
