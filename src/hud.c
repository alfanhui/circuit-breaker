#include "hud.h"
#include "view.h"
#include "game/minimap.h"

/*
 * The boost gauge: a dark back with a fill that shrinks as the gauge drains.
 *
 * Each part is a tiny solid-colour texture that VDP1 stretches onto a rectangle given in screen
 * pixels (jo_sprite_draw_4p_fixed: no perspective, so it stays put when the boost widens the
 * view). Coordinates are from the centre of the screen, +y down.
 *
 * The fill's colour says what the gauge is doing, so it can be read from the corner of the eye:
 *   white       boosting
 *   amber       refilling, or full
 *   dim amber   the pause after a boost, before it refills (cycle.h, CYCLE_REFILL_DELAY)
 *
 * Amber, because nothing else on screen is: it's the opposite of the trails' cyan (it was cyan,
 * and vanished when the cycle stopped facing a trail), and stands out on the navy floor and
 * against the sunset sky's indigo and magenta.
 */

#define BAR_WIDTH (CYCLE_GAUGE_MAX >> 1) // 90 pixels: a full gauge (180) halved, so no division
#define BAR_HEIGHT 4
#define BAR_Y 76	  // below the centre: near the bottom, inside the 224 rows every TV shows
#define BAR_BORDER 1  // the back shows round the fill
// Nearer than anything in the world where the HUD is, so it's drawn on top (VDP1 has no depth
// buffer: SGL draws far to near). The world reaches SGL at 1/4 size: the nearest trail pieces
// (10 world units ahead, TRAIL_MIN_FORWARD) are 2.5 of SGL's, but collision keeps the cycle 25
// from a trail's side, so those are always off the screen's edges. And SGL drops anything nearer
// than its screen distance / 128 (slZdspLevel 7): 2.2 in iso (60 degree view), so 2 vanished there
#define BAR_DEPTH 4

/*
 * The radar (1st person only): the trails round the player, turned so the way the camera faces
 * is up (game/minimap.h works out the lines). Bottom right, clear of the gauge and debug text.
 * Lines are 4-corner sprites with corners A, B, B, A: VDP1 draws a squashed quad as a line, so
 * the same solid-colour textures as the gauge do. Lines are drawn nearer than the disc, so on it.
 *
 * Saturn pixels aren't square: a TV shows the 320 x 224 picture at 4:3, so each pixel is about
 * 1.13 times as tall as it's wide. Drawn square, the disc came out an oval and the map's angles
 * were squashed. So everything's x is stretched by 9/8 (MAP_WIDEN: x + x / 8, as a shift).
 */
#define MAP_WIDEN(x) ((x) + ((x) >> 3))
#define MAP_X 100 // centre, in pixels from the screen's centre: its edge 137 out, inside what CRTs show
#define MAP_Y 64
#define MAP_DEPTH 4		  // the disc...
#define MAP_LINE_DEPTH 3  // ...and what's on it (1st person only: SGL's near limit there is ~1.25)
#define MAP_DISC_SIZE 32  // the disc's texture, stretched to the map's size
#define MAP_TRAILS_MAX 4  // trail colours ready (only player 1's so far)

#define TEXTURE_SIZE 8 // VDP1 textures are a multiple of 8 wide; it's stretched anyway

enum
{
	PART_BACK,
	PART_BOOSTING,
	PART_READY,
	PART_WAITING,
	PART_MAP_DISC,
	PART_MAP_PLAYER,
	PART_MAP_TRAIL, // one per trail colour, MAP_TRAILS_MAX of them
	PART_COUNT = PART_MAP_TRAIL + MAP_TRAILS_MAX
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

// The radar's disc: dark inside, a dim ring round the edge, transparent outside (VDP1 skips
// those pixels). Worked out in half-pixels from the centre (2x + 1 - size) so the circle is
// symmetrical; no square root, just comparing squared distances
static int make_map_disc(void)
{
	static jo_color pixels[MAP_DISC_SIZE * MAP_DISC_SIZE];
	const int outer = MAP_DISC_SIZE * MAP_DISC_SIZE;			  // (2r)^2, r = half the size
	const int inner = (MAP_DISC_SIZE - 3) * (MAP_DISC_SIZE - 3); // the ring is 1.5 texels wide
	jo_img img;

	for (int y = 0; y < MAP_DISC_SIZE; ++y)
		for (int x = 0; x < MAP_DISC_SIZE; ++x)
		{
			int px = 2 * x + 1 - MAP_DISC_SIZE;
			int py = 2 * y + 1 - MAP_DISC_SIZE;
			int d2 = px * px + py * py;

			pixels[y * MAP_DISC_SIZE + x] = d2 >= outer ? JO_COLOR_Transparent
										  : d2 >= inner ? JO_COLOR_RGB(0, 90, 110) // dim teal ring
														: JO_COLOR_RGB(10, 12, 40); // dark navy, like the gauge's back
		}
	img.width = MAP_DISC_SIZE;
	img.height = MAP_DISC_SIZE;
	img.data = pixels;
	return jo_sprite_add(&img);
}

void hud_init(void)
{
	part_sprites[PART_BACK] = make_part(JO_COLOR_RGB(10, 12, 40));		// dark navy
	part_sprites[PART_BOOSTING] = make_part(JO_COLOR_RGB(235, 250, 255)); // near white
	part_sprites[PART_READY] = make_part(JO_COLOR_RGB(255, 170, 0));	// amber
	part_sprites[PART_WAITING] = make_part(JO_COLOR_RGB(110, 70, 0));	// dim amber
	part_sprites[PART_MAP_DISC] = make_map_disc();
	part_sprites[PART_MAP_PLAYER] = make_part(JO_COLOR_RGB(235, 250, 255)); // near white
	for (int i = 0; i < MAP_TRAILS_MAX; ++i)
		part_sprites[PART_MAP_TRAIL + i] = make_part(map_trail_colours[i]);
	minimap_init();
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

// A rectangle from (left, top) to (right, bottom), in pixels from the screen centre
static void draw_part(int part, int left, int top, int right, int bottom)
{
	draw_corners(part, left, top, right, top, right, bottom, left, bottom, BAR_DEPTH);
}

// A line from (x0, y0) to (x1, y1): a quad squashed flat, corners A, B, B, A
static void draw_line(int part, int x0, int y0, int x1, int y1)
{
	draw_corners(part, x0, y0, x1, y1, x1, y1, x0, y0, MAP_LINE_DEPTH);
}

void hud_draw_minimap(world_point eye, const trail *const *trails, int trail_count)
{
	// The camera's facing now (eased mid-turn, so the map swings round with the view), as a unit
	// vector x1024. Heading 0 faces north (-y), turning right goes east (-x): (-sin, -cos)
	const int forward_x = -(slSin(view_rz) >> 6);
	const int forward_y = -(slCos(view_rz) >> 6);
	const int r = MINIMAP_RADIUS + 1; // the disc's edge, just outside the furthest line
	const int rx = MAP_WIDEN(r);	  // 37: wider in pixels, so it's round on a TV
	int n;

	if (trail_count > MAP_TRAILS_MAX)
		trail_count = MAP_TRAILS_MAX;
	n = minimap_build(trails, trail_count, eye, forward_x, forward_y, map_lines, MINIMAP_LINES_MAX);
	draw_corners(PART_MAP_DISC, MAP_X - rx, MAP_Y - r, MAP_X + rx, MAP_Y - r, MAP_X + rx, MAP_Y + r, MAP_X - rx,
				 MAP_Y + r, MAP_DEPTH);
	for (int i = 0; i < n; ++i)
		draw_line(PART_MAP_TRAIL + map_lines[i].trail, MAP_X + MAP_WIDEN(map_lines[i].x0), MAP_Y + map_lines[i].y0,
				  MAP_X + MAP_WIDEN(map_lines[i].x1), MAP_Y + map_lines[i].y1);
	// The player: a chevron at the centre, always pointing up (the way the camera faces)
	draw_line(PART_MAP_PLAYER, MAP_X - 3, MAP_Y + 3, MAP_X, MAP_Y - 4);
	draw_line(PART_MAP_PLAYER, MAP_X, MAP_Y - 4, MAP_X + 3, MAP_Y + 3);
}

void hud_draw_boost(const cycle *c)
{
	const int left = -(BAR_WIDTH >> 1);
	const int fill = c->gauge >> 1; // pixels: 0..BAR_WIDTH
	int part;

	if (c->boost > 0)
		part = PART_BOOSTING;
	else if (c->refill_wait > 0 && c->gauge < CYCLE_GAUGE_MAX)
		part = PART_WAITING;
	else
		part = PART_READY;

	draw_part(PART_BACK, left - BAR_BORDER, BAR_Y - BAR_BORDER, left + BAR_WIDTH + BAR_BORDER,
			  BAR_Y + BAR_HEIGHT + BAR_BORDER);
	if (fill > 0)
		draw_part(part, left, BAR_Y, left + fill, BAR_Y + BAR_HEIGHT);
}
