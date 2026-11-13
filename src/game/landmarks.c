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
 * Where each stands (the header has the frame). The city is out, to +out from 8000 to 14 000,
 * under the CD block's disc: the video output opposite, behind you as you leave the arena; the
 * sound chip and the cartridge to either side; the two video chips between those and
 * the video output they feed. Each is tall enough to show over the arena's walls from its middle
 * (a wall's top is 11 degrees up from there: 4500 at 23 000)
 */
const landmark landmarks[LANDMARK_COUNT] = {
	{0, 0, 1, PLACE_ARENA, 1100, 7000},				 // the control tower, at the arena's middle
	{2000, -23000, 0, PLACE_SCSP, 2600, 7800},
	{3000, 23000, 0, PLACE_BACKUP_RAM, 3200, 7400},
	{-16500, -16500, 1, PLACE_VDP1, 2800, 7200},
	{-16500, 16500, 1, PLACE_VDP2, 3600, 7300},
	{-25000, 0, 1, PLACE_VIDEO, 4200, 8000},
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
		// The slot, the cartridge standing in it, and its label: lit, but one second in every
		// eight it's out (the battery's dying)
		rect(&shapes[0], -3200, 3200, 0, 2600, DARK);
		rect(&shapes[1], -2000, 2000, 2200, 7400, MID);
		rect(&shapes[2], -1400, 1400, 4600, 6600, ((frame >> 6) & 7) == 0 ? GREEN_OUT : GREEN);
		return 3;
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
