#include "landmarks.h"

// The two body colours, a shade either side of the sky's dark violet, and each one's own
#define DARK LANDMARK_RGB(34, 28, 58)
#define MID LANDMARK_RGB(52, 44, 84)
#define AMBER LANDMARK_RGB(96, 66, 36)
#define AMBER_DEEP LANDMARK_RGB(70, 48, 30)
#define GREEN LANDMARK_RGB(44, 96, 72)
#define GREEN_OUT LANDMARK_RGB(30, 56, 48)
#define MAGENTA LANDMARK_RGB(110, 44, 84)
#define BLUE LANDMARK_RGB(40, 62, 120)
#define BLUE_PALE LANDMARK_RGB(52, 78, 140)
#define CLOCK LANDMARK_RGB(130, 60, 50)
#define SCAN LANDMARK_RGB(110, 125, 150)

/*
 * Where each stands (the header has the frame), by where the Saturn has it round Work RAM
 * (docs/WORK_RAM.md, "A bigger city"; game/city.h). Low Work RAM, the city, is out (+out, 12 500
 * to 22 100), its rows across from -4800 (row 0, the plan's north: -across) to +4900 (row F); High
 * Work RAM, planned, is the other side of the arena (-out). A place whose address is below a bank's
 * stands beyond its row 0 edge, one above it beyond its row F edge, nearer its low or high columns
 * by the address:
 *   Backup RAM (0x0018_0000, below Low):  beyond Low's north edge, over its hub (row 0, column 4)
 *   SCSP (0x05A0_0000, above Low):        beyond Low's south-east corner, past its hub (row E, column E)
 *   VDP1, VDP2 (0x05C0_0000, 0x05E0_0000, below High): beyond High's north edge, VDP1 the nearer the arena
 *   the video output (out of VDP2):        beyond High's south-west corner, the far end
 * (The CD block, above Low too, is the disc over its hub: not in this table.) Each is tall enough to
 * show over the arena's walls from its middle (a wall's top is 11 degrees up from there: 4500 at
 * 23 000), and 16 000 to 26 000 from it, as before
 */
const landmark landmarks[LANDMARK_COUNT] = {
	{0, 0, 1, PLACE_ARENA, 1100, 7000},				   // the control tower, at the arena's middle
	{23500, 13000, 1, PLACE_SCSP, 2600, 7800},		   // past Low's south-east corner
	{15500, -20000, 0, PLACE_BACKUP_RAM, 4600, 6800}, // north of Low: a cartridge, wide and low (landmark_shapes)
	{-14500, -20500, 0, PLACE_VDP1, 2800, 7200},	   // north of High, by the arena
	{-20500, -16500, 0, PLACE_VDP2, 3600, 7300},	   // north of High, further out
	{-23500, 13000, 1, PLACE_VIDEO, 4200, 8000},	   // past High's south-west corner: the way out
};

// A sine of 16 steps, +-12: everything that sways goes by it
static const signed char wave[16] = {0, 5, 8, 11, 12, 11, 8, 5, 0, -5, -8, -11, -12, -11, -8, -5};

// An upright rectangle: from a0 to a1 along, z0 to z1 up
static void rect(landmark_shape *s, int a0, int a1, int z0, int z1, unsigned short colour)
{
	s->along[0] = s->along[3] = (short)a0;
	s->along[1] = s->along[2] = (short)a1;
	s->up[0] = s->up[1] = (short)z0;
	s->up[2] = s->up[3] = (short)z1;
	s->colour = colour;
}

int landmark_shapes(int which, unsigned int frame, landmark_shape *shapes)
{
	switch (landmarks[which].place)
	{
	case PLACE_ARENA:
		// A spire, a crossbar near its top, and the clock: a light on for half of every second
		rect(&shapes[0], -300, 300, 0, 7000, MID);
		rect(&shapes[1], -1100, 1100, 5200, 5600, DARK);
		rect(&shapes[2], -200, 200, 6200, 6600, (frame & 32) ? CLOCK : DARK);
		return 3;
	case PLACE_SCSP:
	{
		// Five capacitors on a board. Each one's height sways by the wave, a step every 16
		// frames, each three steps on from the last: a spectrum moving to something slow
		static const short tall[5] = {4200, 6200, 5000, 7000, 4600};

		rect(&shapes[0], -2600, 2600, 0, 900, MID);
		for (int i = 0; i < 5; ++i)
			rect(&shapes[1 + i], -2350 + i * 950, -2350 + i * 950 + 700, 900,
				 tall[i] + wave[((frame >> 4) + (unsigned int)i * 3) & 15] * 60, (i & 1) ? AMBER_DEEP : AMBER);
		return 6;
	}
	case PLACE_BACKUP_RAM:
		/*
		 * A cartridge in its slot, seen side on: wide and low (the user, 2026-11-13: "wider, and
		 * less tall, and looks different than the video output": a box in a box, with a lit
		 * panel, it read as the video output's screen). So a silhouette of its own: the slot's
		 * long base; the cartridge's body with a narrower grip on top (shoulders, as a real
		 * one's); its edge connector showing above the slot, two runs of gold either side of
		 * the key; and the label, a low strip: lit, but one second in every eight it's out (the
		 * battery's dying)
		 */
		// (The slot's a tall block: from the arena the skyline hides everything under ~4100, and
		// a cartridge at the ground, 4200 high, didn't show over it at all: sky_arena.steps)
		rect(&shapes[0], -4600, 4600, 0, 4200, DARK);
		rect(&shapes[1], -3800, 3800, 4200, 6200, MID);
		rect(&shapes[2], -2600, 2600, 6200, 6800, MID);
		rect(&shapes[3], -3300, -400, 4200, 4600, AMBER_DEEP);
		rect(&shapes[4], 400, 3300, 4200, 4600, AMBER_DEEP);
		rect(&shapes[5], -2600, 2600, 5000, 5700, ((frame >> 6) & 7) == 0 ? GREEN_OUT : GREEN);
		return 6;
	case PLACE_VDP1:
	{
		/*
		 * A tilted four-cornered polygon (VDP1 draws them at any angle), and over it the part
		 * filled so far: from its bottom edge up, as VDP1 fills, a line at a time. t goes 8 to
		 * 255 over 8.5 seconds, then it starts again: the fill's top corners are that far from
		 * the bottom ones to the top ones (never 0: a polygon with no height is a stray line)
		 */
		static const short along[4] = {-2600, 1800, 2800, -1600}, up[4] = {1500, 2600, 7200, 6000};
		const int step = (int)((frame >> 1) & 255);
		const int t = step < 8 ? 8 : step;

		for (int k = 0; k < 4; ++k)
		{
			shapes[0].along[k] = along[k];
			shapes[0].up[k] = up[k];
		}
		shapes[0].colour = DARK;
		shapes[1].along[0] = along[0];
		shapes[1].up[0] = up[0];
		shapes[1].along[1] = along[1];
		shapes[1].up[1] = up[1];
		shapes[1].along[2] = (short)(along[1] + (((along[2] - along[1]) * t) >> 8));
		shapes[1].up[2] = (short)(up[1] + (((up[2] - up[1]) * t) >> 8));
		shapes[1].along[3] = (short)(along[0] + (((along[3] - along[0]) * t) >> 8));
		shapes[1].up[3] = (short)(up[0] + (((up[3] - up[0]) * t) >> 8));
		shapes[1].colour = MAGENTA;
		return 2;
	}
	case PLACE_VDP2:
		// Four planes, seen edge on, one over another. Each slides along by the wave, a step
		// every 8 frames, each a quarter of the wave on from the one below: scrolling layers
		for (int i = 0; i < 4; ++i)
		{
			const int slide = wave[((frame >> 3) + (unsigned int)i * 4) & 15] * 50;

			rect(&shapes[i], -3000 + slide, 3000 + slide, 2800 + i * 1400, 3060 + i * 1400, (i & 1) ? BLUE_PALE : BLUE);
		}
		return 4;
	default: // PLACE_VIDEO
	{
		// A bezel, the screen in it, and a dark bar rolling down the screen (a TV filmed by a
		// camera). The screen's glow breathes: its grey up and down by the wave, slowly
		const int glow = 176 + wave[(frame >> 5) & 15] * 2;
		const int bar = 7200 - (int)((frame >> 1) & 255) * 20;

		rect(&shapes[0], -4200, 4200, 1400, 8000, DARK);
		rect(&shapes[1], -3700, 3700, 1900, 7500, LANDMARK_RGB(glow - 24, glow, glow + 24));
		rect(&shapes[2], -3700, 3700, bar, bar + 300, SCAN);
		return 3;
	}
	}
}
