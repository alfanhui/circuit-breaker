#include "turn_ease.h"

int ease_out_step(int current, int target, int share, int min_step)
{
	int remaining = target - current;
	int distance = remaining < 0 ? -remaining : remaining;
	int step;

	if (distance <= min_step)
		return target; // close enough: land on the target exactly

	// A share of what's left: a multiply and a shift by 4 (two 2-bit shifts on the SH-2), no division.
	// Worked on the positive distance, because shifting a negative number rounds the wrong way
	step = (distance * share) >> 4;
	if (step < min_step)
		step = min_step; // keeps the tail from crawling

	return remaining < 0 ? current - step : current + step;
}

int turn_ease_step(int current, int target)
{
	return ease_out_step(current, target, TURN_EASE_SHARE, TURN_EASE_MIN_STEP);
}

int turn_linear_step(int current, int target)
{
	if (current < target - TURN_LINEAR_STEP)
		return current + TURN_LINEAR_STEP;
	if (current > target + TURN_LINEAR_STEP)
		return current - TURN_LINEAR_STEP;
	return target;
}
