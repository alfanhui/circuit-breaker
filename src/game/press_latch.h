#ifndef __PRESS_LATCH_H__
#define __PRESS_LATCH_H__

/*
 * Remembers button presses between the pad read (every vblank, in an interrupt)
 * and the game frame that acts on them. Pure C, so it's unit-tested on the PC.
 *
 * Why: the pad is sampled once per vblank, but a slow game frame can span several
 * vblanks. Comparing only the last two samples then misses a press made in an
 * earlier one, and the turn the player asked for never happens.
 *
 * Interrupt safety, without disabling interrupts: each field has a single writer.
 *   - The vblank (press_latch_sample) is the only writer of held and toggles.
 *   - The game (press_latch_take) is the only writer of seen.
 * A new press flips its bit in toggles; take() reports the bits that differ from seen.
 * Reading a 16-bit value is one instruction on the SH-2, so the game can never
 * see a half-written toggles, and a press can't be wiped out mid-take.
 *
 * Limit: pressing the SAME button twice before one game frame reads it flips the
 * bit back, so it isn't reported. That needs a frame over ~3 vblanks (50 ms) and a
 * double tap inside it.
 *
 * Bits are 1 = pressed (the Saturn pad itself reports 0 = pressed; convert first).
 */

typedef struct
{
	volatile unsigned short held;	 // buttons down at the last sample (vblank writes)
	volatile unsigned short toggles; // flipped once per new press (vblank writes)
	unsigned short seen;			 // toggles as of the last take (game writes)
} press_latch;

void press_latch_init(press_latch *l);

// Vblank: record this sample of the pad (1 = pressed).
void press_latch_sample(press_latch *l, unsigned short pressed_now);

// Game frame: buttons newly pressed since the last call (1 = struck).
unsigned short press_latch_take(press_latch *l);

#endif
