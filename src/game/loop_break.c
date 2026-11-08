#include "loop_break.h"

// Each press harder than the last: the burst's reach in quarters of LOOP_BREAK_BURST_REACH (x1,
// x1.5, x2.5: >> 2 after the multiply), and the flash's strength (out of 255: white)
static const unsigned char reach_quarters[LOOP_BREAK_PRESSES + 1] = {0, 4, 6, 10};
static const unsigned char flash_strength[LOOP_BREAK_PRESSES + 1] = {0, 96, 160, 255};

void loop_break_init(loop_break *b)
{
	b->presses = 0;
	b->frames = 255; // long ago: no burst, no flash
}

int loop_break_press(loop_break *b)
{
	if (b->presses >= LOOP_BREAK_PRESSES)
		return 0;
	b->presses++;
	b->frames = 0;
	return b->presses;
}

void loop_break_update(loop_break *b)
{
	if (b->frames < 255)
		b->frames++;
}

int loop_break_broken(const loop_break *b)
{
	return b->presses >= LOOP_BREAK_PRESSES;
}

int loop_break_burst(const loop_break *b, int *inner, int *outer)
{
	int reach;
	int left; // frames to go

	if (b->presses == 0 || b->frames >= LOOP_BREAK_BURST_FRAMES)
		return 0;
	reach = (LOOP_BREAK_BURST_REACH * reach_quarters[b->presses]) >> 2;
	left = LOOP_BREAK_BURST_FRAMES - b->frames;
	// Ease-out, cubed so it snaps out: by 1 - (left / 16)^3 of the reach. 16^3 is 4096, so >> 12.
	// (Squared, it was under half way a quarter of the way through: too slow for a hit)
	*outer = (reach * (4096 - left * left * left)) >> 12;
	// The inner ends follow, b->frames / 16 of the way out: the rays shorten to nothing
	*inner = (*outer * b->frames) >> 4;
	return 1;
}

int loop_break_flash(const loop_break *b)
{
	if (b->presses == 0 || b->frames >= LOOP_BREAK_FLASH_FRAMES)
		return 0;
	// Fades in a straight line: strength x frames to go / 8
	return (flash_strength[b->presses] * (LOOP_BREAK_FLASH_FRAMES - b->frames)) >> 3;
}
