#include "rounds.h"

// Each round's rivals (STORY.md). Rounds 4 and 5 are the Watchdog's: 1 for now, as it isn't built
static const unsigned char round_rivals[ROUND_COUNT] = {1, 2, 3, 1, 1};

// The status line's words for each round: each round is one more time round the loop
static const char *const round_titles[ROUND_COUNT] = {
	"Loop iteration 1", "Loop iteration 2", "Loop iteration 3", "Loop iteration 4", "Loop iteration 5",
};

static void start_phase(rounds *r, int phase)
{
	r->phase = (unsigned char)phase;
	r->frames = 0;
}

void rounds_init(rounds *r)
{
	r->number = 0;
	start_phase(r, ROUND_PLAYING);
}

void rounds_restart(rounds *r)
{
	start_phase(r, ROUND_PLAYING);
}

int rounds_update(rounds *r, int rivals_alive)
{
	r->frames++;
	switch (r->phase)
	{
	case ROUND_PLAYING:
		if (rivals_alive > 0)
			return ROUND_NOTHING;
		start_phase(r, ROUND_SINKING);
		return ROUND_CLEARED;
	case ROUND_SINKING:
		if (r->frames >= (1 << ROUND_SINK_SHIFT))
			start_phase(r, ROUND_OPEN);
		return ROUND_NOTHING;
	case ROUND_OPEN:
		if (r->frames < ROUND_OPEN_FRAMES)
			return ROUND_NOTHING;
		// The next round's walls start up. Its number changes now, so a layout of its own could
		// rise (STORY.md: each round's arena is different). After the last, the first again
		r->number = (unsigned char)(r->number + 1 < ROUND_COUNT ? r->number + 1 : 0);
		start_phase(r, ROUND_RISING);
		return ROUND_RISES;
	default: // ROUND_RISING
		if (r->frames < (1 << ROUND_RISE_SHIFT))
			return ROUND_NOTHING;
		start_phase(r, ROUND_PLAYING);
		return ROUND_BEGINS;
	}
}

// Smoothly from 0 to ROUND_WALLS_FULL as u goes from 0 to 256: 3u^2 - 2u^3, as the walls rise
// in the arrival (arrival_flight.c), so they ease off the floor and settle, rather than jolt
static int smooth(int u)
{
	if (u <= 0)
		return 0;
	if (u >= 256)
		return ROUND_WALLS_FULL;
	return (u * u * (768 - 2 * u)) >> 16;
}

int rounds_wall_rise(const rounds *r)
{
	switch (r->phase)
	{
	case ROUND_SINKING:
		// frames << (8 - shift): the fraction done, x256, with a shift rather than a division
		return ROUND_WALLS_FULL - smooth(r->frames << (8 - ROUND_SINK_SHIFT));
	case ROUND_OPEN:
		return 0;
	case ROUND_RISING:
		return smooth(r->frames << (8 - ROUND_RISE_SHIFT));
	default:
		return ROUND_WALLS_FULL;
	}
}

int rounds_rivals(const rounds *r)
{
	return round_rivals[r->number];
}

const char *rounds_title(const rounds *r)
{
	return round_titles[r->number];
}
