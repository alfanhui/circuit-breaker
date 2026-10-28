#ifndef __TURN_EASE_H__
#define __TURN_EASE_H__

/*
 * Ease-out for the 1st person camera's turn. Pure C, unit-tested on the PC.
 *
 * The cycle turns instantly (cycle.c); only the camera swings round, so the turn has to *look*
 * instant too. A linear swing (the same 9 degrees every frame) starts as slowly as it ends,
 * which reads as lag. Ease-out covers a fixed share of what's left each frame: a big step at
 * once, then smaller and smaller ones as it settles, like something with weight coming to rest.
 *
 * With a share of 6/16 a 90 degree turn goes:
 *     frame:     1    2    3    4    5    6    7    8    9   10
 *     turned:   38%  61%  76%  85%  90%  94%  96%  98%  99% 100%
 * (the old linear turn was at 10% after frame 1, 40% after frame 4), and it lands in 10 frames,
 * one sooner than the old turn (11: 10 x 1638 falls 4 short of 0x4000). Near the end the steps
 * would shrink for ever, so they never go below TURN_EASE_MIN_STEP, and once within one step
 * the camera snaps onto the heading.
 *
 * Angles are SGL's: 0x10000 is a full circle, 0x4000 is 90 degrees.
 */

#define TURN_EASE_SHARE 6		  // sixteenths of the remaining angle covered each frame
#define TURN_EASE_MIN_STEP 0x0100 // 1.4 degrees: the slowest the camera turns before it lands

// The camera's next angle, one frame from current towards target. Never overshoots.
int turn_ease_step(int current, int target);

// The old turn, kept to compare against (debug: Z in input.c): the same 9 degrees every frame
#define TURN_LINEAR_STEP (0x4000 / 10) // worked out by the compiler, so no division at runtime
int turn_linear_step(int current, int target);

#endif
