#include "hud.h"
#include "view.h"
#include "game/minimap.h"

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
 * screen distance / 128 (slZdspLevel 7): 2.2 in iso (60 degree view), so 2 vanished there
 */
#define MAP_FILL_DEPTH 5
#define MAP_LINE_DEPTH 4
#define GAUGE_DEPTH 3
#define MAP_DISC_SIZE 32 // the glass's texture, stretched to the map's size
#define MAP_TRAILS_MAX 4 // trail colours ready: player 1 and a bot so far
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
	minimap_init();
	prepare_map_disc();
}

// Any 4 corners, in pixels from the screen centre (+y down), at an SGL depth (nearer drawn later)
static void draw_corners(int part, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, int depth)
{
	jo_pos2D_fixed corners[4] = {
		{jo_int2fixed(x0), jo_int2fixed(y0)},
		{jo_int2fixed(x1), jo_int2fixed(y1)},
		{jo_int2fixed(x2), jo_int2fixed(y2)},
		{jo_int2fixed(x3), jo_int2fixed(y3)},
	};

	jo_sprite_draw_4p_fixed(part_sprites[part], corners, jo_int2fixed(depth), true);
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

void hud_draw_minimap(world_point eye, const trail *const *trails, int trail_count)
{
	// The camera's facing now (eased mid-turn, so the map swings round with the view), as a unit
	// vector x1024. Heading 0 faces north (-y), turning right goes east (-x): (-sin, -cos)
	const int forward_x = -(slSin(view_rz) >> 6);
	const int forward_y = -(slCos(view_rz) >> 6);
	int n;

	if (trail_count > MAP_TRAILS_MAX)
		trail_count = MAP_TRAILS_MAX;
	n = minimap_build(trails, trail_count, eye, forward_x, forward_y, map_lines, MINIMAP_LINES_MAX);

	// The glass as a mesh: VDP1 draws every other pixel, in a checkerboard, and the world shows
	// through the gaps. A Saturn's composite or RF picture blurs that into see-through glass (a
	// sharp emulator shows the checkerboard). VDP1's own half-transparency would only blend with
	// what VDP1 has drawn, not the VDP2 floor and sky, so over the floor it would be solid
	jo_sprite_enable_screen_doors_filter();
	draw_map_glass();
	jo_sprite_disable_screen_doors_filter();

	for (int i = 0; i < n; ++i)
		draw_thick_map_line(PART_MAP_TRAIL + map_lines[i].trail, map_lines[i].x0, map_lines[i].y0, map_lines[i].x1,
							map_lines[i].y1);
	// The player: a chevron at the centre, always pointing up (the way the camera faces), lying
	// on the tilted map like everything else
	draw_map_line(PART_MAP_PLAYER, -3, 3, 0, -4);
	draw_map_line(PART_MAP_PLAYER, 0, -4, 3, 3);
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
