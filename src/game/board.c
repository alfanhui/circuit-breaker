#include "board.h"

/*
 * Dull, so the board stays under everything (the art direction's contrast is in the towers and
 * the pulses): near-black with a little teal, copper a shade lighter, the traces darker than
 * the copper. Only a pulse is bright
 */
#define SUBSTRATE BOARD_RGB(0, 1, 2)
#define TRACE_EDGE BOARD_RGB(1, 4, 5)
#define PAD BOARD_RGB(5, 9, 10)
#define HOLE BOARD_RGB(0, 0, 1)
#define TRACE BOARD_RGB(2, 6, 7)

// A pulse, head first
static const unsigned short pulse[BOARD_PULSE_LENGTH] = {
	BOARD_RGB(14, 27, 30), // the head: a pale cyan
	BOARD_RGB(7, 17, 20),
	BOARD_RGB(4, 11, 13),
};

unsigned short board_colour(int board_class, unsigned int step)
{
	switch (board_class)
	{
	case BOARD_SUBSTRATE:
		return SUBSTRATE;
	case BOARD_TRACE_EDGE:
		return TRACE_EDGE;
	case BOARD_PAD:
		return PAD;
	case BOARD_HOLE:
		return HOLE;
	default:
	{
		// How far behind the nearest pulse's head this phase is (the heads are on phase `step`, and
		// move on to higher phases): the period's a power of two, so it's a mask, not a division.
		// Behind by under the pulse's length: lit
		const unsigned int behind = (step - (unsigned int)(board_class - BOARD_FIRST_PHASE)) & (BOARD_PULSE_PERIOD - 1);

		return behind < BOARD_PULSE_LENGTH ? pulse[behind] : TRACE;
	}
	}
}

void board_palette(unsigned short *palette, unsigned int step)
{
	// Class by class, the same colour for each of its grid shades: no division by the shades
	int i = 0;

	for (int c = 0; c < BOARD_CLASSES; ++c)
	{
		const unsigned short colour = board_colour(c, step);

		for (int s = 0; s < BOARD_GRID_SHADES; ++s)
			palette[i++] = colour;
	}
}
