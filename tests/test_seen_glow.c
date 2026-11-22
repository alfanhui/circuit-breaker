#include "test.h"
#include "../src/game/seen_glow.h"

// Nothing at first; full red in 8 frames seen; out over 32 frames once not
static void rises_fast_and_fades_slowly(void)
{
	seen_glow g;
	int r, gr, b;

	seen_glow_init(&g);
	seen_glow_offset(&g, &r, &gr, &b);
	CHECK_EQ(0, r | gr | b);
	for (int i = 0; i < 8; ++i)
		seen_glow_update(&g, true);
	seen_glow_offset(&g, &r, &gr, &b);
	CHECK_EQ(SEEN_GLOW_RED, r);
	CHECK_EQ(-SEEN_GLOW_COOL, gr);
	CHECK_EQ(-SEEN_GLOW_COOL, b);
	seen_glow_update(&g, true); // held at its full
	CHECK_EQ(SEEN_GLOW_FULL, g.level);
	for (int i = 0; i < 16; ++i)
		seen_glow_update(&g, false);
	seen_glow_offset(&g, &r, &gr, &b);
	CHECK(r > 0 && r < SEEN_GLOW_RED); // halfway out, a quarter second on
	for (int i = 0; i < 16; ++i)
		seen_glow_update(&g, false);
	CHECK_EQ(0, g.level); // gone in half a second
	seen_glow_offset(&g, &r, &gr, &b);
	CHECK_EQ(0, r | gr | b);
}

void run_seen_glow_tests(void)
{
	rises_fast_and_fades_slowly();
}
