#include "hud.h"

/*
 * The boost gauge: a dark back with a fill that shrinks as the gauge drains.
 *
 * Each part is a tiny solid-colour texture that VDP1 stretches onto a rectangle given in screen
 * pixels (jo_sprite_draw_4p_fixed: no perspective, so it stays put when the boost widens the
 * view). Coordinates are from the centre of the screen, +y down.
 *
 * The fill's colour says what the gauge is doing, so it can be read from the corner of the eye:
 *   white       boosting
 *   cyan        refilling, or full: the trail's colour
 *   dim teal    the pause after a boost, before it refills (cycle.h, CYCLE_REFILL_DELAY)
 */

#define BAR_WIDTH (CYCLE_GAUGE_MAX >> 1) // 90 pixels: a full gauge (180) halved, so no division
#define BAR_HEIGHT 4
#define BAR_Y 76	  // below the centre: near the bottom, inside the 224 rows every TV shows
#define BAR_BORDER 1  // the back shows round the fill
// Nearer than anything in the world, so it's drawn on top: the nearest trail and wall pieces are
// 20 world units = 5 of SGL's (the world reaches SGL at 1/4 size). But SGL drops anything nearer
// than its screen distance / 128 (slZdspLevel 7): 2.2 in iso (60 degree view), so 2 vanished there
#define BAR_DEPTH 4

#define TEXTURE_SIZE 8 // VDP1 textures are a multiple of 8 wide; it's stretched anyway

enum
{
	PART_BACK,
	PART_BOOSTING,
	PART_READY,
	PART_WAITING,
	PART_COUNT
};

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

void hud_init(void)
{
	part_sprites[PART_BACK] = make_part(JO_COLOR_RGB(10, 12, 40));		// dark navy
	part_sprites[PART_BOOSTING] = make_part(JO_COLOR_RGB(235, 250, 255)); // near white
	part_sprites[PART_READY] = make_part(JO_COLOR_RGB(0, 220, 255));	// the trail's cyan
	part_sprites[PART_WAITING] = make_part(JO_COLOR_RGB(0, 90, 110));	// dim teal
}

// A rectangle from (left, top) to (right, bottom), in pixels from the screen centre
static void draw_part(int part, int left, int top, int right, int bottom)
{
	jo_pos2D_fixed corners[4] = {
		{jo_int2fixed(left), jo_int2fixed(top)},	 // top-left
		{jo_int2fixed(right), jo_int2fixed(top)},	 // top-right
		{jo_int2fixed(right), jo_int2fixed(bottom)}, // bottom-right
		{jo_int2fixed(left), jo_int2fixed(bottom)},	 // bottom-left
	};

	jo_sprite_draw_4p_fixed(part_sprites[part], corners, jo_int2fixed(BAR_DEPTH), true);
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
