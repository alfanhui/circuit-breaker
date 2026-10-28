#include "turn_ease.h"

int turn_ease_step(int current, int target)
{
	int remaining = target - current;
	int distance = remaining < 0 ? -remaining : remaining;
	int step;

	if (distance <= TURN_EASE_MIN_STEP)
		return target; // close enough: land on the heading exactly

	// A share of what's left: a multiply and a shift by 4 (two 2-bit shifts on the SH-2), no division.
	// Worked on the positive distance, because shifting a negative number rounds the wrong way
	step = (distance * TURN_EASE_SHARE) >> 4;
	if (step < TURN_EASE_MIN_STEP)
		step = TURN_EASE_MIN_STEP; // keeps the tail from crawling

	return remaining < 0 ? current - step : current + step;
}

int turn_linear_step(int current, int target)
{
	if (current < target - TURN_LINEAR_STEP)
		return current + TURN_LINEAR_STEP;
	if (current > target + TURN_LINEAR_STEP)
		return current - TURN_LINEAR_STEP;
	return target;
}
