#ifndef __LOOP_BREAK_H__
#define __LOOP_BREAK_H__

/*
 * Breaking the prologue's loop (docs/STORY.md). Pure C, unit-tested on the PC.
 *
 * While the cycle writes its message by itself, the player presses a direction. Each press hits
 * the loop like a blow in Zelda: The Wind Waker: a burst of light out from the cycle, the whole
 * screen flashing white, a jolt of the camera and a dark, electric synth stab. The loop holds
 * for two of them, each harder than the last; the third forces the choice, and it breaks.
 *
 * This counts the presses and times the light: how big the burst is and how bright the flash,
 * frame by frame. Drawing and sound are elsewhere (main.c, input.c).
 *
 * The burst: rays from the cycle shoot out to LOOP_BREAK_BURST_REACH pixels (more for each
 * press), snapping out and slowing (a cubed ease-out), while their inner ends chase after them, so the
 * ring thins away to nothing in LOOP_BREAK_BURST_FRAMES. The flash: the screen brightened by
 * the press's strength, fading in LOOP_BREAK_FLASH_FRAMES. All with multiplies and shifts.
 */

#define LOOP_BREAK_PRESSES 3	   // the third breaks it
#define LOOP_BREAK_BURST_FRAMES 16 // the burst's life: 16, so >> 4 turns frames into sixteenths
#define LOOP_BREAK_FLASH_FRAMES 8  // and the flash's: >> 3
#define LOOP_BREAK_BURST_REACH 48  // pixels, the first press's burst: the second x 1.5, the third x 2.5

typedef struct
{
	unsigned char presses; // 0 until the first; LOOP_BREAK_PRESSES once it's broken
	unsigned char frames;  // since the last press
} loop_break;

void loop_break_init(loop_break *b);

// A direction pressed: returns which press this is (1, 2, or 3: broken). Presses after the
// third are ignored (it returns 0)
int loop_break_press(loop_break *b);

// Each frame
void loop_break_update(loop_break *b);

// 1 once broken
int loop_break_broken(const loop_break *b);

// The burst this frame, in pixels from the cycle: rays from *inner to *outer. 0 when there's none
int loop_break_burst(const loop_break *b, int *inner, int *outer);

// How much brighter the screen is this frame: 0 (not at all) to 255 (white), for VDP2's colour
// offset
int loop_break_flash(const loop_break *b);

#endif
