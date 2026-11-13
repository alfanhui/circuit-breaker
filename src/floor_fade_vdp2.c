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
 * (An HBlank *interrupt* doing the same hung at start-up under SGL: see SKILLS.md.)
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

/*
 * Which of the SCU's three DMA levels the fog has to itself. jo-engine 2026 (no SGL) uses level
 * 0, every frame, to copy the VDP1 command table (jo's vdp1_command_pipeline.c,
 * JO_VDP1_SCU_DMA_LEVEL), and nothing else: CD reads go through the SH-2's own DMA, and the sound
 * isn't streamed by DMA. So level 1. (Under SGL it was level 0: SGL streamed sound on level 2
 * and used level 1 for its own copies; on level 2, every sound stopped the fog till it ended.)
 * Level 0 goes first when two levels want the bus at once: an HBlank falling while jo copies
 * the command table waits for it (docs/HARDWARE_TESTS.md)
 */
#define FOG_DMA_LEVEL 1
#define PHYSICAL(address) ((unsigned int)(address) & 0x07FFFFFF)
// The level's registers (SCU manual): jo's JO_SCU_DMA_xxx(level), each level 0x20 bytes on
#define DMA_ENABLE_ON (1 << 8)						 // D1EN: DXEN, starts on its start factor (bit 0, DXGO, starts it now)
#define DMA_MODE_HBLANK ((1 << 16) | (0 << 8) | 2) // D1MD: direct; the read address moves on (DXRUP), the write one
													 // goes back (DXWUP 0); started by each HBlank-in (factor 2)

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

// Must match view.c's spectator camera: 25 degrees below level, 9000 from the cycle along its
// line of sight (3804 up, 8157 back), through a 30 degree telephoto FOV. The top of the screen
// sees the floor ~14800 away; the fog starts past the cycle, so the play stays clear and the far
// floor dims into the dark, as if the arena went on for ever
static const floor_fade_view spectator_view = {
	.centre_row = JO_TV_HEIGHT_2,
	.tilt = 466,		 // 25 degrees below level: tan 25 = 0.466
	.eye_height = 3804,	 // 9000 x sin 25
	.focal = 597,		 // 30 degree FOV: 160 / tan(15)
	.fade_start = 9700,	 // 1500 past the cycle (8157 away along the floor)
	.fade_end = 20000,	 // past the top of the screen: about half strength there
	.glow_rows = 0,
};

static const floor_fade_view *const views[] = {&first_person_view, &iso_view, &spectator_view};

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

/*
 * Stopping the DMA safely, before its registers are written (SEGA's SCU final precautions):
 *   - 23: a level's mode (D0MD) and address steps (D0AD) mustn't be written while it's
 *     transferring: the Saturn hangs. Clearing the enable stops new transfers starting, not one
 *     already running, and that was all arm_dma did before writing them
 *   - 13: the DMA status register that would say whether one's running was deleted
 *   - 22: a transfer started by an HBlank must end within its line (4 bytes take far less)
 * So: clear the enable, then wait for the row counter to move on twice. The first move may come
 * straight after, but the second is a whole line later: nothing can still be running by then.
 * ~1.5 lines on average (~95 us, ~0.6% of a frame), once a frame in VBlank. Bounded, in case the
 * counter ever stands still: a line is ~1700 CPU cycles, a few dozen of these reads
 */
#define ROW_WAIT_READS 2000

static void stop_dma(void)
{
	unsigned int row;
	int moves = 0;

	JO_SCU_DMA_ENABLE(FOG_DMA_LEVEL) = 0;
	row = current_row();
	for (int reads = 0; moves < 2 && reads < ROW_WAIT_READS; ++reads)
	{
		const unsigned int now = current_row();

		if (now != row)
		{
			row = now;
			++moves;
		}
	}
}

/*
 * The DMA: one 4-byte entry per HBlank, from the table (the read address moves on, ready for
 * the next row) to CCRR and CCRLB, 2 bytes apart (CCRLB holds the line colour and back
 * screen ratios, which this blend doesn't use). The write address goes back each time.
 * from: the entry the next HBlank copies. Six register writes. Only after stop_dma: never while
 * it might be transferring. The table's in Work RAM-H (the game's static data), which the SCU's
 * DMA can read; it can't read Work RAM-L (precaution 4)
 */
static void arm_dma(const unsigned int *from)
{
	JO_SCU_DMA_READ(FOG_DMA_LEVEL) = PHYSICAL(from);
	JO_SCU_DMA_WRITE(FOG_DMA_LEVEL) = VDP2_CCRR_PHYSICAL;
	JO_SCU_DMA_COUNT(FOG_DMA_LEVEL) = sizeof(dma_table[0]); // the whole entry, so the read address lands on the next one
	JO_SCU_DMA_ADD(FOG_DMA_LEVEL) = JO_SCU_DMA_ADD_READ4_WRITE2; // reads 4 bytes at a time, writes 2
	JO_SCU_DMA_MODE(FOG_DMA_LEVEL) = DMA_MODE_HBLANK;
	JO_SCU_DMA_ENABLE(FOG_DMA_LEVEL) = DMA_ENABLE_ON; // armed: starts on every HBlank from now on
}

void floor_fade_init(void)
{
	// One colour RAM entry: the colour every line blends towards
	jo_create_palette(&fog_palette);
	fog_palette.data[0] = JO_COLOR_RGB(FOG_R, FOG_G, FOG_B);
	// Line colour entries are colour RAM numbers: the palette's offset from the start of CRAM
	unsigned short fog_colour = (unsigned short)(((unsigned int)fog_palette.data - JO_VDP2_CRAM) / sizeof(jo_color));

	// Not JO_VDP2_LINE_COLOR_TABLE: under SGL jo handed that out from the start of VRAM bank B1,
	// where the text layer (jo_printf) kept its font, and the table overwrote the glyphs. In the
	// floor's own bank, which jo's allocator tracks
	line_table = (unsigned short *)jo_vdp2_malloc(JO_VDP2_RAM_CELL_RBG0, JO_TV_HEIGHT * sizeof(unsigned short));
	if (line_table == JO_NULL)
		return; // no room: no fog (line_table stays NULL, so the rest does nothing)
	for (int row = 0; row < JO_TV_HEIGHT; ++row)
		line_table[row] = fog_colour; // the same colour on every row: only the ratio varies
	floor_fade_set_view(FLOOR_VIEW_ISO);

	// (Under SGL, jo set the floor's coefficient table up to give the line colour too, all 0 =
	// black, and it had to be registered again without that. jo 2026 doesn't: jo's vdp2.c,
	// JO_VDP2_KTCTL_VALUE.)
	// jo 2026 has no calls for the line colour screen, so its two registers are written here
	// (VDP2 manual). jo keeps a copy of the registers it sets and writes them in VBlank, but only
	// the ones it changes, and it never changes these two:
	//   LCTA: where the table is (a VRAM offset in 2-byte words); bit 15, one colour per row
	{
		const unsigned int words = ((unsigned int)line_table - JO_VDP2_VRAM) >> 1;

		JO_VDP2_LCTAU = (unsigned short)(0x8000 | (words >> 16));
		JO_VDP2_LCTAL = (unsigned short)(words & 0xFFFF);
	}
	//   LNCLEN: which screens get the line colour inserted under them: RBG0 (bit 4), the floor
	JO_VDP2_LNCLEN = 1 << 4;
	// ...and the floor blended with it by RBG0's ratio (CCRR, set per row by the DMA below): jo's
	// own call (ratio mode, the top screen's ratio), as SGL's slColorCalc(CC_RATE | CC_TOP | RBG0ON)
	jo_enable_screen_transparency(JO_RBG0_SCREEN, 0);

	stop_dma(); // not running yet: in case anything left this level going
	arm_dma(dma_table);
}

void floor_fade_vblank(void)
{
	unsigned int left, row;

	if (line_table == JO_NULL)
		return;
	// The whole channel, not just its read address: under SGL a CD read set up this same level
	// for itself and left it that way. Six register writes a frame, so nothing can stop the fog
	// for good. Stopped first, safely (stop_dma), which takes a line or two: the row's read after it.
	// It returns just after the row counter moves on, so the row read here is a fresh one: an
	// HBlank between reading it and arming the DMA (the fog a row out for that frame) is less
	// likely than before, not ruled out (where in a line the counter moves isn't known here)
	stop_dma();
	row = current_row();
	left = hblanks_before_picture(row);
	if (left <= MAX_BLANK_ROWS)
		// The HBlanks still to come before row 0 (at the end of this row and every one after it)
		// use the last that many padding entries
		arm_dma(&dma_table[MAX_BLANK_ROWS - left]);
	else
		// Still in the picture (shouldn't happen in a VBlank callback): row r's HBlank sets up
		// row r + 1, from entry MAX_BLANK_ROWS + r. Stopped, it has to be started again here
		arm_dma(&dma_table[MAX_BLANK_ROWS + (row < JO_TV_HEIGHT ? row : 0)]);
}

void floor_fade_set_colour(jo_color colour)
{
	// One colour RAM write, and only when it changes (kept here: reading colour RAM back waits on
	// VDP2): the line colour table names this entry on every row, so the whole fog turns at once
	static jo_color now = JO_COLOR_RGB(FOG_R, FOG_G, FOG_B);

	if (line_table != JO_NULL && colour != now)
	{
		now = colour;
		fog_palette.data[0] = colour;
	}
}

void floor_fade_distances(int view_index, int *fade_start, int *fade_end)
{
	const floor_fade_view *view = views[view_index];

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

void floor_fade_set_view(int view)
{
	rebuild(views[view]);
}

void floor_fade_set_first_person_tilt(int tilt)
{
	floor_fade_view view = first_person_view;

	view.tilt = tilt;
	rebuild(&view);
}

void floor_fade_set_camera(int tilt, int eye_height, int focal, int fade_start, int fade_end)
{
	floor_fade_view view;

	view.centre_row = JO_TV_HEIGHT_2;
	view.tilt = tilt;
	view.eye_height = eye_height;
	view.focal = focal;
	view.fade_start = fade_start;
	view.fade_end = fade_end;
	view.glow_rows = 0;
	rebuild(&view);
}
