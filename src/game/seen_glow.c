#include "seen_glow.h"

void seen_glow_init(seen_glow *g)
{
	g->level = 0;
}

void seen_glow_update(seen_glow *g, bool seen)
{
	if (seen)
		g->level = (unsigned char)(g->level + SEEN_GLOW_RISE > SEEN_GLOW_FULL ? SEEN_GLOW_FULL : g->level + SEEN_GLOW_RISE);
	else
		g->level = (unsigned char)(g->level > SEEN_GLOW_FALL ? g->level - SEEN_GLOW_FALL : 0);
}

void seen_glow_offset(const seen_glow *g, int *red, int *green, int *blue)
{
	// Its share of the full tint: level / 32, a shift (SEEN_GLOW_FULL's a power of two)
	*red = (SEEN_GLOW_RED * g->level) >> 5;
	*green = *blue = -((SEEN_GLOW_COOL * g->level) >> 5);
}
