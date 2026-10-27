#include "floor_fade_vdp2.h"
#include "game/floor_fade.h"

// The haze is added on top of the floor, so this is what the horizon tends towards:
// the pink of the sky's horizon, so floor and sky meet in one glow.
#define HAZE_R 230
#define HAZE_G 100
#define HAZE_B 150

// Must match view.c's 1st person camera
static const floor_fade_view first_person_view = {
	.centre_row = JO_TV_HEIGHT_2,
	.tilt = 0, // level: the horizon is the projection centre
	.eye_height = 35,
	.focal = JO_TV_WIDTH_2, // 90 degree FOV: (320 / 2) / tan(45) = 160
	.fade_start = 300, // grid lines are thinner than a pixel from ~270 away (screen row ~133)
	.fade_end = 2000,
	.glow_rows = 6,
};

// Must match view.c's iso camera. Its horizon is off the top of the screen, but the far, upper
// part of the floor is where the 45 degree grid lines merge into moire: fade that instead
static const floor_fade_view iso_view = {
	.centre_row = JO_TV_HEIGHT_2,
	.tilt = 1192,		  // ISO_RX 40 degrees from straight down = 50 below level: tan 50 = 1.192
	.eye_height = 768,	  // -ISO_Z
	.focal = 277,		  // 60 degree FOV: 160 / tan(30)
	.fade_start = 700,	  // just past the cycle (644 away), so the play area stays clear
	.fade_end = 2000,	  // well past the top of the screen (~1444): about half strength there
	.glow_rows = 0,
};

static jo_palette haze_palette;
static unsigned short first_haze_colour; // colour RAM number of level 0
static unsigned short *line_table;		 // one colour RAM number per screen line, in VDP2 VRAM

void floor_fade_init(void)
{
	// Palette entry i is the haze scaled by i / (levels - 1): entry 0 is black, so adds nothing
	jo_create_palette(&haze_palette);
	for (int i = 0; i < FLOOR_FADE_LEVELS; ++i)
		haze_palette.data[i] = JO_COLOR_RGB(HAZE_R * i / (FLOOR_FADE_LEVELS - 1),
											HAZE_G * i / (FLOOR_FADE_LEVELS - 1),
											HAZE_B * i / (FLOOR_FADE_LEVELS - 1));
	// Line colour entries are colour RAM numbers: the palette's offset from the start of CRAM
	first_haze_colour = (unsigned short)(((unsigned int)haze_palette.data - JO_VDP2_CRAM) / sizeof(jo_color));

	// Not JO_VDP2_LINE_COLOR_TABLE: jo hands that out from the start of VRAM bank B1, where
	// SGL's text layer (jo_printf) keeps its font, so the table overwrote the glyphs.
	// Bank A0 holds the floor's texture with ~60 KB spare, and jo's allocator tracks it.
	line_table = (unsigned short *)jo_vdp2_malloc(JO_VDP2_RAM_CELL_RBG0, JO_TV_HEIGHT * sizeof(unsigned short));
	floor_fade_set_first_person(false);

	// jo sets up the floor's rotation coefficient table with K_LINECOL, which makes VDP2 take
	// the floor's line colour from each coefficient (all 0 = black) instead of our per-line
	// table. Re-register it without that flag; everything else as jo has it (see jo's vdp2.c).
	// (jo_vdp2_malloc for the K table just returns its fixed address in bank A1.)
	void *k_table = jo_vdp2_malloc_autosize(JO_VDP2_RAM_KTABLE);
	slKtableRA(k_table, K_FIX | K_DOT | K_2WORD | K_ON);
	slKtableRB(k_table, K_FIX | K_DOT | K_2WORD | K_ON);

	slLineColTable(line_table);
	slLineColDisp(RBG0ON);					 // insert the line colour under the floor/sky layer...
	slColorCalc(CC_ADD | CC_TOP | RBG0ON);	 // ...and add it on (also enables colour calc on RBG0)
}

void floor_fade_set_first_person(bool first_person)
{
	unsigned char levels[JO_TV_HEIGHT];

	if (line_table == JO_NULL)
		return;
	floor_fade_rows(first_person ? &first_person_view : &iso_view, levels, JO_TV_HEIGHT);

	for (int row = 0; row < JO_TV_HEIGHT; ++row)
		line_table[row] = first_haze_colour + levels[row];
}
