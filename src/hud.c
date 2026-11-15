#include "hud.h"
#include "view.h"
#include "game/minimap.h"
#include "game/arena.h"
#include "input.h" // arena_wall_rise

/*
 * The HUD: one instrument at the bottom centre of the screen. A radar of the trails round the
 * player (1st person only), lying tilted back like a hologram disc, and round its rim the boost
 * gauge (both views).
 *
 * Everything is tiny solid-colour textures that VDP1 stretches onto 4 corners given in screen
 * pixels (jo_sprite_draw_4p_fixed: no perspective of its own, so it stays put when the boost
 * widens the view). Coordinates are from the centre of the screen, +y down.
 *
 * The gauge's colour says what it's doing, so it can be read from the corner of the eye:
 *   white       boosting
 *   amber       refilling, or full
 *   dim amber   the pause after a boost, before it refills (cycle.h, CYCLE_REFILL_DELAY)
 *   dark navy   the empty part of the rim
 * Amber, because nothing else on screen is: it's the opposite of the trails' cyan (it was cyan,
 * and vanished when the cycle stopped facing a trail), and stands out on the navy floor and
 * against the sunset sky's indigo and magenta.
 *
 * The radar: the trails round the player, turned so the way the camera faces is up
 * (game/minimap.h works out the lines and the tilt). Lines are 4-corner sprites with corners
 * A, B, B, A: VDP1 draws a squashed quad as a line, so the same solid-colour textures do.
 *
 * Saturn pixels aren't square: a TV shows the 320 x 224 picture at 4:3, so each pixel is about
 * 1.13 times as tall as it's wide. Drawn square, the disc came out an oval and the map's angles
 * were squashed. So everything's x is stretched by 9/8 (MAP_WIDEN: x + x / 8, as a shift).
 */
#define MAP_WIDEN(x) ((x) + ((x) >> 3))
#define MAP_X 0	 // centre, in pixels from the screen's centre
#define MAP_Y 70 // below the centre: near the bottom, inside the 224 rows every TV shows
/*
 * Drawn far to near (VDP1 has no depth buffer: SGL draws far to near): the glass, the trail
 * lines, then the gauge over the line ends at the rim. All nearer than anything in the world
 * where the HUD is: the world reaches SGL at 1/4 size, and the nearest trail pieces (10 world
 * units ahead, TRAIL_MIN_FORWARD) are 2.5 of SGL's, but collision keeps the cycle 25 from a
 * trail's side, so those are always off the screen's edges. SGL drops anything nearer than its
 * screen distance / 128 (slZdspLevel 7): 2.2 in iso (60 degree view), so 2 vanished there.
 * Depths are SGL's jo_fixed (16.16): whole numbers apart, but the markers need 2.5
 */
#define MAP_FILL_DEPTH (5 * JO_FIXED_1)
#define MAP_LINE_DEPTH (4 * JO_FIXED_1)
#define GAUGE_DEPTH (3 * JO_FIXED_1)
#define MARKER_DEPTH (5 * JO_FIXED_1 / 2) // 2.5: over the gauge, and still drawn in iso (2 isn't)
#define GLITCH_DEPTH (12 * JO_FIXED_1 / 5) // 2.4: over everything else, the radar and gauge too
#define MAP_DISC_SIZE 32 // the glass's texture, stretched to the map's size
#define MAP_TRAILS_MAX 4 // trail colours: player 1 and up to 3 bots (input.h: FIELD_TRAIL_COUNT)
/*
 * The tilted disc in perspective (game/minimap.h). VDP1 stretches a texture across a quad
 * linearly, not in perspective, so one quad bent the circle. So the glass is cut into MAP_STRIPS
 * bands, each its own texture on its own band of the perspective: piecewise straight, close to
 * the true curve. The gauge is 2 x MINIMAP_GAUGE_STEPS segments whose corners are each put
 * through the perspective. Every sprite call costs ~700 cycles, so these are counted: 8 + 24
 */
#define MAP_STRIPS 8
#define MAP_STRIP_ROWS (MAP_DISC_SIZE / MAP_STRIPS) // 4 texture rows per band

#define TEXTURE_SIZE 8 // VDP1 textures are a multiple of 8 wide; it's stretched anyway

enum
{
	PART_BACK,
	PART_BOOSTING,
	PART_READY,
	PART_WAITING,
	PART_MAP_PLAYER,
	PART_MAP_TRAIL,							   // one per trail colour, MAP_TRAILS_MAX of them
	PART_MAP_FILL = PART_MAP_TRAIL + MAP_TRAILS_MAX, // one per band of the glass, MAP_STRIPS of them
	PART_TEXT_CURSOR = PART_MAP_FILL + MAP_STRIPS,	 // the terminal's block cursor
	PART_BURST,										 // the prologue's burst of light
	PART_MAP_WALL,									 // the arena's walls and blocks on the map
	PART_GLITCH_DARK,								 // the designed crash's tear bands (breach.h)
	PART_GLITCH_PINK,
	PART_LOCK, // the head's lock on a target (game/target.h)
	PART_BENCH_PASS, // the benchmark's verdict, the whole screen (game/bench.h)
	PART_BENCH_FAIL,
	PART_COUNT
};

// Each trail's colour on the map. Another player's goes in the next slot
static const jo_color map_trail_colours[MAP_TRAILS_MAX] = {
	JO_COLOR_RGB(0, 220, 255), // player 1: the trail's cyan
	JO_COLOR_RGB(255, 120, 0),
	JO_COLOR_RGB(255, 40, 200),
	JO_COLOR_RGB(120, 255, 60),
};

static minimap_line map_lines[MINIMAP_LINES_MAX];

static int part_sprites[PART_COUNT];

// Worked out once, in hud_init: where the glass's bands start and end (map rows), and the
// gauge's corners on the screen, per side (0 right, 1 left), inner and outer edge, bottom to top
static signed char strip_rows[MAP_STRIPS + 1];
static short gauge_inner[2][MINIMAP_GAUGE_STEPS + 1][2];
static short gauge_outer[2][MINIMAP_GAUGE_STEPS + 1][2];

// One solid colour, TEXTURE_SIZE square. jo copies it into VDP1 RAM, so the buffer is reused
static int make_part(jo_color colour)
{
	static jo_color pixels[TEXTURE_SIZE * TEXTURE_SIZE];
	jo_img img;

	for (int i = 0; i < TEXTURE_SIZE * TEXTURE_SIZE; ++i)
		pixels[i] = colour;
	img.width = TEXTURE_SIZE;
	img.height = TEXTURE_SIZE;
	img.data = pixels;
	return jo_sprite_add(&img);
}

// One band of the radar's glass: texture rows [band * MAP_STRIP_ROWS, + MAP_STRIP_ROWS), deep
// teal inside the circle, transparent outside (VDP1 skips those pixels). Worked out in
// half-pixels from the centre (2x + 1 - size) so the circle is symmetrical; no square root,
// just comparing squared distances
static int make_map_strip(int band)
{
	static jo_color pixels[MAP_DISC_SIZE * MAP_STRIP_ROWS];
	const int outer = MAP_DISC_SIZE * MAP_DISC_SIZE; // (2r)^2, r = half the size
	jo_img img;

	for (int row = 0; row < MAP_STRIP_ROWS; ++row)
		for (int x = 0; x < MAP_DISC_SIZE; ++x)
		{
			int px = 2 * x + 1 - MAP_DISC_SIZE;
			int py = 2 * (band * MAP_STRIP_ROWS + row) + 1 - MAP_DISC_SIZE;

			pixels[row * MAP_DISC_SIZE + x] = px * px + py * py < outer ? JO_COLOR_RGB(0, 50, 70) // deep teal glass
																		 : JO_COLOR_Transparent;
		}
	img.width = MAP_DISC_SIZE;
	img.height = MAP_STRIP_ROWS;
	img.data = pixels;
	return jo_sprite_add(&img);
}

// A map point (pixels from the map's centre, flat) to the screen: tilted back (minimap_tilt),
// widened for the TV, then moved to where the map sits
static void map_to_screen(int x, int y, int *screen_x, int *screen_y)
{
	minimap_tilt(x, y, screen_x, screen_y);
	*screen_x = MAP_X + MAP_WIDEN(*screen_x);
	*screen_y += MAP_Y;
}

// The disc's fixed shape, once minimap_init has made the perspective table
static void prepare_map_disc(void)
{
	const int e = MINIMAP_EDGE;

	// Band k covers map rows from -e + 2e * k / MAP_STRIPS down: the texture's rows, spread
	// across the disc's 2e + 1 map rows (start-up, so the division is fine)
	for (int k = 0; k <= MAP_STRIPS; ++k)
		strip_rows[k] = (signed char)(-e + (2 * e * k) / MAP_STRIPS);
	// The gauge's corners: up each side of the rim, put through the perspective
	for (int k = 0; k <= MINIMAP_GAUGE_STEPS; ++k)
		for (int side = 0; side < 2; ++side)
		{
			int x, y, sx, sy;

			minimap_gauge_point(k, MINIMAP_GAUGE_INNER, &x, &y);
			map_to_screen(side ? -x : x, y, &sx, &sy);
			gauge_inner[side][k][0] = (short)sx;
			gauge_inner[side][k][1] = (short)sy;
			minimap_gauge_point(k, MINIMAP_GAUGE_OUTER, &x, &y);
			map_to_screen(side ? -x : x, y, &sx, &sy);
			gauge_outer[side][k][0] = (short)sx;
			gauge_outer[side][k][1] = (short)sy;
		}
}

void hud_init(void)
{
	part_sprites[PART_BACK] = make_part(JO_COLOR_RGB(10, 12, 40));		// dark navy
	part_sprites[PART_BOOSTING] = make_part(JO_COLOR_RGB(235, 250, 255)); // near white
	part_sprites[PART_READY] = make_part(JO_COLOR_RGB(255, 170, 0));	// amber
	part_sprites[PART_WAITING] = make_part(JO_COLOR_RGB(110, 70, 0));	// dim amber
	part_sprites[PART_MAP_PLAYER] = make_part(JO_COLOR_RGB(235, 250, 255)); // near white
	for (int i = 0; i < MAP_TRAILS_MAX; ++i)
		part_sprites[PART_MAP_TRAIL + i] = make_part(map_trail_colours[i]);
	for (int k = 0; k < MAP_STRIPS; ++k)
		part_sprites[PART_MAP_FILL + k] = make_map_strip(k);
	part_sprites[PART_TEXT_CURSOR] = make_part(HUD_TERMINAL_GREEN);
	part_sprites[PART_BURST] = make_part(JO_COLOR_RGB(255, 255, 255));
	part_sprites[PART_MAP_WALL] = make_part(JO_COLOR_RGB(170, 150, 255)); // pale violet: not any rider's
	part_sprites[PART_GLITCH_DARK] = make_part(JO_COLOR_RGB(8, 0, 16));  // near black (all 0 would be see-through)
	part_sprites[PART_GLITCH_PINK] = make_part(JO_COLOR_RGB(255, 40, 160)); // the flicker's pink
	part_sprites[PART_LOCK] = make_part(JO_COLOR_RGB(255, 170, 0)); // amber, as the gauge: yours, not the world's
	part_sprites[PART_BENCH_PASS] = make_part(JO_COLOR_RGB(0, 150, 40)); // green: tests/e2e/run.sh reads it
	part_sprites[PART_BENCH_FAIL] = make_part(JO_COLOR_RGB(170, 0, 20)); // red
	minimap_init();
	prepare_map_disc();
}

// Any 4 corners, in pixels from the screen centre (+y down), at an SGL depth (jo_fixed; nearer drawn later)
static void draw_corners(int part, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, jo_fixed depth)
{
	jo_pos2D_fixed corners[4] = {
		{jo_int2fixed(x0), jo_int2fixed(y0)},
		{jo_int2fixed(x1), jo_int2fixed(y1)},
		{jo_int2fixed(x2), jo_int2fixed(y2)},
		{jo_int2fixed(x3), jo_int2fixed(y3)},
	};

	jo_sprite_draw_4p_fixed(part_sprites[part], corners, depth, true);
}

// A line between two map points: a quad squashed flat, corners A, B, B, A
static void draw_map_line(int part, int x0, int y0, int x1, int y1)
{
	int ax, ay, bx, by;

	map_to_screen(x0, y0, &ax, &ay);
	map_to_screen(x1, y1, &bx, &by);
	draw_corners(part, ax, ay, bx, by, bx, by, ax, ay, MAP_LINE_DEPTH);
}

// A line 2 pixels thick: VDP1's lines are 1 pixel, so it's drawn twice, side by side. The second
// is 1 pixel across the line's length: to the right of a steep line, below a shallow one
static void draw_thick_map_line(int part, int x0, int y0, int x1, int y1)
{
	int ax, ay, bx, by;
	int dx, dy, step_x, step_y;

	map_to_screen(x0, y0, &ax, &ay);
	map_to_screen(x1, y1, &bx, &by);
	dx = bx - ax;
	dy = by - ay;
	step_x = (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx); // steep: step across in x
	step_y = !step_x;
	draw_corners(part, ax, ay, bx, by, bx, by, ax, ay, MAP_LINE_DEPTH);
	draw_corners(part, ax + step_x, ay + step_y, bx + step_x, by + step_y, bx + step_x, by + step_y, ax + step_x,
				 ay + step_y, MAP_LINE_DEPTH);
}

// The glass: each band onto its own band of the perspective (VDP1 stretches each texture to fit)
static void draw_map_glass(void)
{
	const int e = MINIMAP_EDGE;
	int x0, y0, x1, y1, x2, y2, x3, y3;

	for (int k = 0; k < MAP_STRIPS; ++k)
	{
		map_to_screen(-e, strip_rows[k], &x0, &y0);		// far left
		map_to_screen(e, strip_rows[k], &x1, &y1);		// far right
		map_to_screen(e, strip_rows[k + 1], &x2, &y2);	// near right
		map_to_screen(-e, strip_rows[k + 1], &x3, &y3); // near left
		draw_corners(PART_MAP_FILL + k, x0, y0, x1, y1, x2, y2, x3, y3, MAP_FILL_DEPTH);
	}
}

// A point t sixteenths of the way from corner k to corner k + 1 (t 0..16)
static void gauge_lerp(const short corners[][2], int k, int t, int *x, int *y)
{
	*x = corners[k][0] + (((corners[k + 1][0] - corners[k][0]) * t) >> 4);
	*y = corners[k][1] + (((corners[k + 1][1] - corners[k][1]) * t) >> 4);
}

// Part of segment k on one side of the rim, from t0 to t1 sixteenths of the way along it
static void draw_gauge_piece(int part, int side, int k, int t0, int t1)
{
	int ix0, iy0, ox0, oy0, ox1, oy1, ix1, iy1;

	gauge_lerp(gauge_inner[side], k, t0, &ix0, &iy0);
	gauge_lerp(gauge_outer[side], k, t0, &ox0, &oy0);
	gauge_lerp(gauge_outer[side], k, t1, &ox1, &oy1);
	gauge_lerp(gauge_inner[side], k, t1, &ix1, &iy1);
	draw_corners(part, ix0, iy0, ox0, oy0, ox1, oy1, ix1, iy1, GAUGE_DEPTH);
}

void hud_draw_text_cursor(int column, int row)
{
	// The text cell's top left, in pixels from the screen's centre. Text cells are 8 x 8; VDP1
	// draws a quad's edges inclusive, so corners 7 apart fill exactly 8 pixels
	int left = column * 8 - JO_TV_WIDTH_2;
	int top = row * 8 - JO_TV_HEIGHT_2;

	draw_corners(PART_TEXT_CURSOR, left, top, left + 7, top, left + 7, top + 7, left, top + 7, GAUGE_DEPTH);
}

/*
 * A marker on the rim pointing at a rider (game/minimap.h, minimap_direction): an arrowhead
 * standing out from the rim at the nearest of its 15 degree points, in the rider's trail colour.
 * It stands outside the rim rather than on it, so it doesn't merge with the amber gauge.
 * Its shape comes from the rim's own corners there: from the inner corner out to the outer one
 * is "outwards" (r), and r turned 90 degrees is "along the rim" (its width). Outer + r is the tip
 */
static void draw_rim_marker(int part, int side, int k)
{
	const int ix = gauge_inner[side][k][0];
	const int iy = gauge_inner[side][k][1];
	const int ox = gauge_outer[side][k][0];
	const int oy = gauge_outer[side][k][1];
	const int rx = ox - ix; // ~3 pixels, outwards
	const int ry = oy - iy;

	// Base corners either side of the outer corner (1.5 r each way: ~9 pixels across), the tip 3 r
	// further out, and back to the middle. At r it was too small to spot beside the amber gauge
	const int wx = (3 * ry) >> 1;
	const int wy = (3 * rx) >> 1;

	draw_corners(part, ox - wx, oy + wy, ox + 3 * rx, oy + 3 * ry, ox + wx, oy - wy, ox, oy, MARKER_DEPTH);
}

void hud_draw_minimap(world_point eye, const trail *const *trails, int trail_count, const hud_marker *markers,
					  int marker_count)
{
	// The camera's facing now (eased mid-turn, so the map swings round with the view), as a unit
	// vector x1024. Heading 0 faces north (-y), turning right goes east (-x): (-sin, -cos)
	const int forward_x = -(jo_sin_angle(view_rz) >> 6);
	const int forward_y = -(jo_cos_angle(view_rz) >> 6);
	int n;
	int walls;
	int block_count;

	if (trail_count > MAP_TRAILS_MAX)
		trail_count = MAP_TRAILS_MAX;
	n = minimap_build(trails, trail_count, eye, forward_x, forward_y, map_lines, MINIMAP_LINES_MAX);
	// The walls round you, as you need to know them weaving between the blocks (and as the last
	// round closes in). Thin lines, drawn first: under the trails, as structure not riders
	// Between rounds, sunk flat into the floor, there's nothing there
	walls = 0;
	if (arena_wall_rise() > 0)
	{
		const world_rect *blocks = arena_blocks(&block_count);

		walls = minimap_build_walls(blocks, block_count, arena_half(), eye, forward_x, forward_y, &map_lines[n],
									MINIMAP_LINES_MAX - n);
	}

	// The glass as a mesh: VDP1 draws every other pixel, in a checkerboard, and the world shows
	// through the gaps. A Saturn's composite or RF picture blurs that into see-through glass (a
	// sharp emulator shows the checkerboard). VDP1's own half-transparency would only blend with
	// what VDP1 has drawn, not the VDP2 floor and sky, so over the floor it would be solid
	jo_sprite_enable_screen_doors_filter();
	draw_map_glass();
	jo_sprite_disable_screen_doors_filter();

	for (int i = n; i < n + walls; ++i)
		draw_map_line(PART_MAP_WALL, map_lines[i].x0, map_lines[i].y0, map_lines[i].x1, map_lines[i].y1);
	for (int i = 0; i < n; ++i)
		draw_thick_map_line(PART_MAP_TRAIL + map_lines[i].trail, map_lines[i].x0, map_lines[i].y0, map_lines[i].x1,
							map_lines[i].y1);
	// The player: a chevron at the centre, always pointing up (the way the camera faces), lying
	// on the tilted map like everything else
	draw_map_line(PART_MAP_PLAYER, -3, 3, 0, -4);
	draw_map_line(PART_MAP_PLAYER, 0, -4, 3, 3);

	// Which way each rider is, however far: on the rim, turned with the camera like the map
	for (int i = 0; i < marker_count; ++i)
	{
		int side, k;

		minimap_direction(markers[i].position.x - eye.x, markers[i].position.y - eye.y, forward_x, forward_y, &side,
						  &k);
		draw_rim_marker(PART_MAP_TRAIL + (markers[i].colour < MAP_TRAILS_MAX ? markers[i].colour : 0), side, k);
	}
}

void hud_draw_boost(const cycle *c)
{
	// How much of each side is lit, in 16ths of a segment: a multiply and a shift (minimap.h)
	const int lit = MINIMAP_GAUGE_LIT(c->gauge, CYCLE_GAUGE_MAX);
	int part;

	if (c->boost > 0)
		part = PART_BOOSTING;
	else if (c->refill_wait > 0 && c->gauge < CYCLE_GAUGE_MAX)
		part = PART_WAITING;
	else
		part = PART_READY;

	// Both sides, bottom to top: lit up to `lit`, dark after. The segment the edge falls in is
	// drawn as two pieces
	for (int side = 0; side < 2; ++side)
		for (int k = 0; k < MINIMAP_GAUGE_STEPS; ++k)
		{
			int start = k << 4; // this segment's start, in 16ths

			if (lit >= start + 16)
				draw_gauge_piece(part, side, k, 0, 16);
			else if (lit <= start)
				draw_gauge_piece(PART_BACK, side, k, 0, 16);
			else
			{
				draw_gauge_piece(part, side, k, 0, lit - start);
				draw_gauge_piece(PART_BACK, side, k, lit - start, 16);
			}
		}
}

/*
 * The prologue's burst of light (game/loop_break.h): BURST_RAYS rays round (cx, cy), from inner to
 * outer pixels out, every other one shorter, like the star of a hit in The Wind Waker. Each ray
 * is a thin spike: BURST_WIDTH across at its inner end, to a point at its outer end (a quad with
 * its two outer corners together).
 * The directions are a table, x64 (the ray's length then >> 6): no trigonometry at runtime
 */
#define BURST_RAYS 12
#define BURST_WIDTH 2 // pixels either side of the ray at its inner end
// Deeper than the rest of the HUD: SGL drops anything nearer than its screen distance / 128
// (slZdspLevel 7), and through the spectator camera's 30 degree FOV that's 160 / tan 15 / 128 =
// 4.7. At the gauge's 3 the burst vanished while spectating. Still far nearer than the world
#define BURST_DEPTH (6 * JO_FIXED_1)
static const signed char burst_directions[BURST_RAYS][2] = {
	// cos, sin x 64, every 30 degrees, starting 15 degrees round so none is level or upright
	{62, 17}, {45, 45}, {17, 62}, {-17, 62}, {-45, 45}, {-62, 17},
	{-62, -17}, {-45, -45}, {-17, -62}, {17, -62}, {45, -45}, {62, -17},
};

void hud_draw_burst(int cx, int cy, int inner, int outer)
{
	for (int i = 0; i < BURST_RAYS; ++i)
	{
		const int dx = burst_directions[i][0];
		const int dy = burst_directions[i][1];
		const int reach = (i & 1) ? (outer * 3) >> 2 : outer; // every other ray three quarters as long
		const int start = (i & 1) ? (inner * 3) >> 2 : inner;
		// Across the ray: its direction turned 90 degrees, x BURST_WIDTH / 64
		const int wx = (-dy * BURST_WIDTH) >> 6;
		const int wy = (dx * BURST_WIDTH) >> 6;
		const int ix = cx + ((dx * start) >> 6);
		const int iy = cy + ((dy * start) >> 6);
		const int ox = cx + ((dx * reach) >> 6);
		const int oy = cy + ((dy * reach) >> 6);

		if (reach <= start)
			continue;
		// Corners: inner left, outer, outer, inner right; at least a pixel wide where it starts
		draw_corners(PART_BURST, ix + wx - (wx == 0), iy + wy - (wy == 0), ox, oy, ox, oy, ix - wx + (wx == 0),
					 iy - wy + (wy == 0), BURST_DEPTH);
	}
}

void hud_draw_band(int top, int height, bool pink)
{
	// Screen pixels from the centre: the whole width, top to top + height - 1 (edges inclusive)
	const int y0 = top - JO_TV_HEIGHT_2;
	const int y1 = y0 + height - 1;
	const int x0 = -JO_TV_WIDTH_2;
	const int x1 = JO_TV_WIDTH_2 - 1;

	draw_corners(pink ? PART_GLITCH_PINK : PART_GLITCH_DARK, x0, y0, x1, y0, x1, y1, x0, y1, GLITCH_DEPTH);
}

/*
 * The benchmark's verdict (game/bench.h): the whole screen green or red, over everything but the
 * text (VDP2's text layer is in front of VDP1's), so a film reads it from the picture's colour
 */
void hud_draw_verdict(int pass)
{
	const int x = JO_TV_WIDTH_2 + 8, y = JO_TV_HEIGHT_2 + 8; // a little past each edge

	draw_corners(pass ? PART_BENCH_PASS : PART_BENCH_FAIL, -x, -y, x, -y, x, y, -x, y, GLITCH_DEPTH);
}

void hud_draw_panel(int column, int row, int columns, int rows)
{
	// Text cells are 8 x 8 (a shift: no multiply needed); a 4 pixel margin all round
	const int x0 = (column << 3) - JO_TV_WIDTH_2 - 4, y0 = (row << 3) - JO_TV_HEIGHT_2 - 4;
	const int x1 = x0 + (columns << 3) + 7, y1 = y0 + (rows << 3) + 7;

	draw_corners(PART_GLITCH_DARK, x0, y0, x1, y0, x1, y1, x0, y1, GLITCH_DEPTH);
}

/*
 * The lock: four corners of a square round the target, each two short bars (2 pixels thick, so
 * they hold up on a TV), LOCK_HALF from the middle with arms LOCK_ARM long. 8 quads: a corner's
 * bars are drawn from its point inwards, along x and along y
 */
#define LOCK_HALF 12
#define LOCK_ARM 5

void hud_draw_lock(int cx, int cy)
{
	for (int corner = 0; corner < 4; ++corner)
	{
		const int sx = (corner & 1) ? 1 : -1, sy = (corner & 2) ? 1 : -1; // which way out it is
		const int x = cx + sx * LOCK_HALF, y = cy + sy * LOCK_HALF;		  // its point
		const int ax = x - sx * LOCK_ARM, ay = y - sy * LOCK_ARM;		  // its arms' inner ends
		const int tx = x - sx, ty = y - sy;								  // a pixel in: the thickness

		draw_corners(PART_LOCK, x, y, ax, y, ax, ty, x, ty, MARKER_DEPTH);
		draw_corners(PART_LOCK, x, y, x, ay, tx, ay, tx, y, MARKER_DEPTH);
	}
}
