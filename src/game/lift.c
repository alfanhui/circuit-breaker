#include "lift.h"

void lift_init(lift *l, int low, int high)
{
	l->state = LIFT_WAITING;
	l->frames = 0;
	l->low = (short)low;
	l->high = (short)high;
}

static void enter(lift *l, int state)
{
	l->state = (unsigned char)state;
	l->frames = 0;
}

void lift_update(lift *l, bool on_pad)
{
	switch (l->state)
	{
	case LIFT_WAITING:
		if (on_pad)
		{
			enter(l, LIFT_ARMING);
			l->frames = 1; // this frame counts: stood on it LIFT_ARM_FRAMES frames, it goes
		}
		break;
	case LIFT_ARMING:
		if (!on_pad)
			enter(l, LIFT_WAITING); // only passing over it
		else if (++l->frames >= LIFT_ARM_FRAMES)
			enter(l, LIFT_UP);
		break;
	case LIFT_UP:
		if (++l->frames >= LIFT_RIDE_FRAMES)
			enter(l, LIFT_HELD);
		break;
	case LIFT_HELD:
		if (++l->frames >= LIFT_HOLD_FRAMES)
			enter(l, LIFT_DOWN);
		break;
	case LIFT_DOWN:
		if (++l->frames >= LIFT_RIDE_FRAMES)
			enter(l, LIFT_ARRIVED);
		break;
	default: // LIFT_ARRIVED
		if (!on_pad)
			enter(l, LIFT_WAITING);
		break;
	}
}

/*
 * How far through a ride, 0 .. LIFT_EASE_ONE, eased: smoothstep, 3t^2 - 2t^3. It leaves and
 * arrives at no speed, so the eye isn't jerked (a lift that set off at full speed felt like a
 * teleport). t is the frame over the ride's frames, a power of two, so it's all shifts:
 * f^2 (3F - 2f) is 0 .. F^3 (2^27 for a 512-frame ride: inside 32 bits), brought down to 15 bits.
 * Fine enough that the 5250 it's multiplied by moves a sixth of a unit at a time (at 10 bits the
 * pad climbed in jumps of 5)
 */
#define LIFT_EASE_SHIFT 15
#define LIFT_EASE_ONE (1 << LIFT_EASE_SHIFT)
static int eased(int frames)
{
	return (frames * frames * (3 * LIFT_RIDE_FRAMES - 2 * frames)) >> (3 * LIFT_RIDE_SHIFT - LIFT_EASE_SHIFT);
}

int lift_height(const lift *l)
{
	const int rise = l->high - l->low;

	switch (l->state)
	{
	case LIFT_UP:
		// rise is under 2^13 and the ease 2^15: inside 32 bits
		return l->low + ((rise * eased(l->frames)) >> LIFT_EASE_SHIFT);
	case LIFT_HELD:
		return l->high;
	case LIFT_DOWN:
		return l->high - ((rise * eased(l->frames)) >> LIFT_EASE_SHIFT);
	default:
		return l->low;
	}
}

bool lift_carries(const lift *l)
{
	return l->state == LIFT_UP || l->state == LIFT_HELD || l->state == LIFT_DOWN;
}
