#ifndef __LIFT_H__
#define __LIFT_H__

#include <stdbool.h>

/*
 * A hub's lift (docs/WORK_RAM.md, "Hubs and lifts"): the way out of the city to the area the hub
 * belongs to. The first one: the CD block's, up to the disc in the sky over its hub (game/city.h).
 *
 * There's no button for it (the pad has none left: walker.h). You stand on its pad, and after a
 * moment it goes: long enough that walking across the pad doesn't take you. It carries you up,
 * easing away and easing in, holds at the top, and brings you back down (for now: the area up
 * there isn't built, so the top is a look at it). Back down, it won't go again until you've
 * stepped off the pad: or you'd never get off it.
 *
 * Only the lift's own state and height: input.c holds the walker on the pad while it carries.
 * Pure C: unit-tested on the PC (tests/test_lift.c). No division: the ride's ease is a table-free
 * smoothstep on a power of two of frames
 */

#define LIFT_WAITING 0 // nobody on its pad
#define LIFT_ARMING 1  // someone's stood on the pad: about to go
#define LIFT_UP 2	   // carrying you up
#define LIFT_HELD 3	   // at the top
#define LIFT_DOWN 4	   // carrying you back
#define LIFT_ARRIVED 5 // down again: waits for you to step off

#define LIFT_ARM_FRAMES 45	  // stood on the pad this long (0.75 s) and it goes
#define LIFT_RIDE_SHIFT 9	  // a ride is 512 frames (8.5 s): 5250 up, ~10 a frame, 15 at its fastest
#define LIFT_RIDE_FRAMES (1 << LIFT_RIDE_SHIFT)
#define LIFT_HOLD_FRAMES 240 // at the top: 4 s to look

typedef struct
{
	unsigned char state;
	unsigned short frames; // into the state
	short low, high;	   // the pad's height at the bottom and the top
} lift;

void lift_init(lift *l, int low, int high);

// One frame. on_pad: you're stood on the pad (only matters while it's not carrying you)
void lift_update(lift *l, bool on_pad);

// The pad's height now
int lift_height(const lift *l);

// Whether it has you: going up, held at the top or coming down. You're kept on the pad
bool lift_carries(const lift *l);

#endif
