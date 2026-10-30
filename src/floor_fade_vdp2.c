#include "floor_fade_vdp2.h"
#include "game/floor_fade.h"

/*
 * Fog to black on the floor (VDP2's rotating background, RBG0), one strength per screen row.
 *
 * VDP2 can mix a screen with a "line colour" (one colour per screen row, from a table) in two
 * ways: add it on, or blend the two by a ratio. Adding can only brighten, so it fogs to a glow,
 * never to black. Blending by a ratio with black scales the floor down: grid lines and navy
 * darken together, which is real fog. But the ratio is one register (CCRR) for the whole
 * screen, not one per row...
 *
 * ...so it's rewritten on every row, without the CPU. At the end of each row VDP2 has a short
 * gap, horizontal blank (HBlank), and the SCU's DMA can be set to start a transfer on every
 * HBlank. Each transfer copies the next row's ratio from a table in RAM into CCRR, then leaves
 * its read address on the following entry, ready for the next row. The DMA reads RAM 4 bytes
 * at a time, and a transfer that stops part-way through those 4 leaves the read address
 * part-way too, so the rows drift: each row's entry is 4 bytes (the ratio twice) and each
 * transfer moves all 4, to CCRR and the register after it.
 * (An HBlank *interrupt* doing the same hung at start-up and was masked again by SGL: see SKILLS.md.)
 *
 * The catch: HBlank happens on the blank rows between frames too, and the DMA keeps moving
 * through the table on those. So the table starts with padding (row 0's ratio), and every frame
 * a VBlank callback reads VDP2's row counter and points the DMA far enough into the padding
 * that the blank rows still to come use up exactly the rest of it. That works from the row
 * counter alone, whatever the picture height: jo's JO_TV_HEIGHT says 240, but the picture
 * is 224 rows in Mednafen (39 blank, not 23), and assuming 240 put the fog 16 rows out.
 */

// The fog colour every far row blends towards
#define FOG_R 0
#define FOG_G 0
#define FOG_B 0

// VDP2 registers (VDP2 manual). The CPU uses the 0x2... cache-through addresses; the SCU's DMA
// takes plain physical addresses (the top bits dropped)
#define VDP2_EXTEN ((volatile unsigned short *)0x25F80002)	 // reading it latches HCNT/VCNT
#define VDP2_VCNT ((volatile unsigned short *)0x25F8000A)	 // the row counter, as latched
#define VDP2_CCRR_PHYSICAL 0x05F8010C						 // RBG0's colour calculation ratio

// SCU DMA level 2 registers (SCU manual): the level SGL is least likely to use
#define SCU_D2R ((volatile unsigned int *)0x25FE0040)  // read address
#define SCU_D2W ((volatile unsigned int *)0x25FE0044)  // write address
#define SCU_D2C ((volatile unsigned int *)0x25FE0048)  // bytes per transfer
#define SCU_D2AD ((volatile unsigned int *)0x25FE004C) // address steps
#define SCU_D2EN ((volatile unsigned int *)0x25FE0050) // enable
#define SCU_D2MD ((volatile unsigned int *)0x25FE0054) // mode and start factor
#define PHYSICAL(address) ((unsigned int)(address) & 0x07FFFFFF)

// NTSC: 263 rows a frame. The picture is 224 or 240 of them, so at most 39 are blank
#define FRAME_ROWS 263
#define MAX_BLANK_ROWS (FRAME_ROWS - 224)

// Must match view.c's 1st person camera
static const floor_fade_view first_person_view = {
	.centre_row = JO_TV_HEIGHT_2,
	.tilt = 0, // level: the horizon is the projection centre
	.eye_height = 35,
	.focal = JO_TV_WIDTH_2, // 90 degree FOV: (320 / 2) / tan(45) = 160
	.fade_start = 400,		// grid lines are thinner than a pixel from ~270 away (screen row ~133)
	.fade_end = 3000,		// black from here: the far walls stand on darkness
	.glow_rows = 0,			// nothing above the horizon to fog
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

static jo_palette fog_palette;
static unsigned short *line_table; // one colour RAM number per screen line, in VDP2 VRAM

// What the DMA copies, one entry per HBlank:
//   [0, MAX_BLANK_ROWS)      padding, row 0's ratio: copied at the end of the blank rows, so the
//                            last one sets up row 0
//   MAX_BLANK_ROWS + r       row r + 1's ratio, copied at the end of row r
//   after the last row       row 0's ratio again, in case a restart is ever late
static unsigned int dma_table[MAX_BLANK_ROWS + JO_TV_HEIGHT + 8];


// VDP2's row counter. Rows of the picture count from 0; the blank rows below it carry on
// counting, but during vertical sync the counter jumps up so that it reaches 511 on the last
// row before the picture and wraps to 0 (Mednafen's VDP2, which follows the hardware).
// Reading EXTEN is what latches the counter (reading TVSTAT doesn't)
static unsigned int current_row(void)
{
	(void)*VDP2_EXTEN;
	return *VDP2_VCNT & 0x1FF;
}

// How many HBlanks are still to come before row 0 is drawn, counted from blank row `row`
static unsigned int hblanks_before_picture(unsigned int row)
{
	return row >= 0x100 ? 0x200 - row : FRAME_ROWS - row; // in vertical sync, or before it
}

void floor_fade_init(void)
{
	// One colour RAM entry: the colour every line blends towards
	jo_create_palette(&fog_palette);
	fog_palette.data[0] = JO_COLOR_RGB(FOG_R, FOG_G, FOG_B);
	// Line colour entries are colour RAM numbers: the palette's offset from the start of CRAM
	unsigned short fog_colour = (unsigned short)(((unsigned int)fog_palette.data - JO_VDP2_CRAM) / sizeof(jo_color));

	// Not JO_VDP2_LINE_COLOR_TABLE: jo hands that out from the start of VRAM bank B1, where
	// SGL's text layer (jo_printf) keeps its font, so the table overwrote the glyphs.
	// Bank A0 holds the floor's texture with ~60 KB spare, and jo's allocator tracks it.
	line_table = (unsigned short *)jo_vdp2_malloc(JO_VDP2_RAM_CELL_RBG0, JO_TV_HEIGHT * sizeof(unsigned short));
	for (int row = 0; row < JO_TV_HEIGHT; ++row)
		line_table[row] = fog_colour; // the same colour on every row: only the ratio varies
	floor_fade_set_first_person(false);

	// jo sets up the floor's rotation coefficient table with K_LINECOL, which makes VDP2 take
	// the floor's line colour from each coefficient (all 0 = black) instead of our per-line
	// table. Re-register it without that flag; everything else as jo has it (see jo's vdp2.c).
	// (jo_vdp2_malloc for the K table just returns its fixed address in bank A1.)
	void *k_table = jo_vdp2_malloc_autosize(JO_VDP2_RAM_KTABLE);
	slKtableRA(k_table, K_FIX | K_DOT | K_2WORD | K_ON);
	slKtableRB(k_table, K_FIX | K_DOT | K_2WORD | K_ON);

	slLineColTable(line_table);
	slLineColDisp(RBG0ON);					// insert the line colour under the floor...
	slColorCalc(CC_RATE | CC_TOP | RBG0ON); // ...and blend by RBG0's ratio (CCRR), set per row below

	// The DMA: one 4-byte entry per HBlank, from the table (the read address moves on, ready for
	// the next row) to CCRR and CCRLB, 2 bytes apart (CCRLB holds the line colour and back
	// screen ratios, which this blend doesn't use). The write address goes back each time
	*SCU_D2EN = 0;
	*SCU_D2R = PHYSICAL(dma_table);
	*SCU_D2W = VDP2_CCRR_PHYSICAL;
	*SCU_D2C = sizeof(dma_table[0]); // the whole entry, so the read address lands on the next one
	*SCU_D2AD = DMA_SCU_R4 | DMA_SCU_W2;
	*SCU_D2MD = DMA_SCU_DIR | (DMA_SCU_REN << DMA_DXMD_DXRUP) | (DMA_SCU_KEEP << DMA_DXMD_DXWUP) | DMA_SCU_F_HBLK_IN;
	*SCU_D2EN = 1 << DMA_DXEN_DXEN; // armed: starts on every HBlank from now on
}

void floor_fade_vblank(void)
{
	unsigned int left;

	if (line_table == JO_NULL)
		return;
	// The HBlanks still to come before row 0 (at the end of this row and every one after it)
	// use the last that many padding entries
	left = hblanks_before_picture(current_row());
	if (left > MAX_BLANK_ROWS)
		return; // still in the picture (shouldn't happen in a VBlank callback): leave it running
	*SCU_D2R = PHYSICAL(&dma_table[MAX_BLANK_ROWS - left]);
}

void floor_fade_distances(bool first_person, int *fade_start, int *fade_end)
{
	const floor_fade_view *view = first_person ? &first_person_view : &iso_view;

	*fade_start = view->fade_start;
	*fade_end = view->fade_end;
}

static void rebuild(const floor_fade_view *view)
{
	unsigned char levels[JO_TV_HEIGHT];

	if (line_table == JO_NULL)
		return;
	floor_fade_rows(view, levels, JO_TV_HEIGHT);

	// Laid out as described at dma_table
	for (unsigned int e = 0; e < sizeof(dma_table) / sizeof(dma_table[0]); ++e)
	{
		unsigned int row = e < MAX_BLANK_ROWS ? 0 : e - MAX_BLANK_ROWS + 1; // the row this entry sets up
		unsigned int ratio = (unsigned int)floor_fade_ratio(levels[row < JO_TV_HEIGHT ? row : 0]);
		dma_table[e] = ratio << 16 | ratio; // both halves: see the top of this file
	}
}

void floor_fade_set_first_person(bool first_person)
{
	rebuild(first_person ? &first_person_view : &iso_view);
}

void floor_fade_set_first_person_tilt(int tilt)
{
	floor_fade_view view = first_person_view;

	view.tilt = tilt;
	rebuild(&view);
}
