#include "press_latch.h"

void press_latch_init(press_latch *l)
{
	l->held = 0;
	l->toggles = 0;
	l->seen = 0;
}

void press_latch_sample(press_latch *l, unsigned short pressed_now)
{
	// Down now but not at the last sample: a new press. Holding a button isn't a new press.
	unsigned short newly_pressed = pressed_now & (unsigned short)~l->held;

	l->toggles ^= newly_pressed;
	l->held = pressed_now;
}

unsigned short press_latch_take(press_latch *l)
{
	unsigned short toggles = l->toggles; // read once: the vblank may change it at any moment
	unsigned short struck = toggles ^ l->seen;

	l->seen = toggles;
	return struck;
}
