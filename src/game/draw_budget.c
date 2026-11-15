#include "draw_budget.h"

void draw_budget_init(draw_budget *b)
{
	b->dropped = 0;
	b->clean = 0;
	b->hold = BUDGET_HOLD_FRAMES;
	b->since_return = -1;
}

void draw_budget_frame(draw_budget *b, int vdp1_unfinished)
{
	if (vdp1_unfinished)
	{
		// A part brought back not long ago: that's what tipped it over. Wait twice as long before
		// trying again (a shift, not a multiply by a variable: the SH-2 does it in one go)
		if (b->since_return >= 0 && b->since_return < BUDGET_HOLD_FRAMES)
		{
			b->hold <<= 1;
			if (b->hold > BUDGET_HOLD_MAX)
				b->hold = BUDGET_HOLD_MAX;
		}
		b->since_return = -1;
		b->clean = 0;
		if (b->dropped < BUDGET_PARTS)
			++b->dropped; // one more part left out, from this frame on
		return;
	}

	// A finished frame
	++b->clean;
	if (b->since_return >= 0 && ++b->since_return >= BUDGET_HOLD_FRAMES)
	{
		// The part brought back has held: the next try needn't wait as long
		b->since_return = -1;
		b->hold >>= 1;
		if (b->hold < BUDGET_HOLD_FRAMES)
			b->hold = BUDGET_HOLD_FRAMES;
	}
	if (b->dropped > 0 && b->clean >= b->hold)
	{
		--b->dropped; // the last part left out comes back (the nearest of those out)
		b->clean = 0;
		b->since_return = 0; // on trial
	}
}

int draw_budget_allows(const draw_budget *b, budget_part part)
{
	return (int)part >= b->dropped;
}
