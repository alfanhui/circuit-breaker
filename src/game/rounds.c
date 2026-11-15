#include "rounds.h"
#include "arena.h"
#include "bot.h"

// Each round's rivals (STORY.md). Rounds 4 and 5 are the Watchdog's: 1 for now, as it isn't built
// Round 4, the rooms: three, one in each of the other rooms (arena.h, arena_rival_start; the
// user, 2026-11-13: "In Level 4 of area, add 3 other bots to each of the quadrants")
static const unsigned char round_rivals[ROUND_COUNT] = {1, 2, 3, 3, 1};

// Each round's arena (STORY.md): a different layout each time round, the last the first's again
static const unsigned char round_layouts[ROUND_COUNT] = {ARENA_OPEN, ARENA_DONUT, ARENA_PILLARS, ARENA_ROOMS,
														 ARENA_OPEN};

// Each round's least bot skill: Medium for the last two
static const unsigned char round_min_skills[ROUND_COUNT] = {BOT_EASY, BOT_EASY, BOT_EASY, BOT_MEDIUM, BOT_MEDIUM};

// The status line's words for each round: each round is one more time round the loop
static const char *const round_titles[ROUND_COUNT] = {
	"Loop iteration 1", "Loop iteration 2", "Loop iteration 3", "Loop iteration 4", "Loop iteration 5",
};

static void start_phase(rounds *r, int phase)
{
	r->phase = (unsigned char)phase;
	r->frames = 0;
	r->dead = 0;
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
	if (r->frames < 0xffff) // held there, not wrapped back to 0: round 5 would open out again
		r->frames++;
	switch (r->phase)
	{
	case ROUND_PLAYING:
		if (rivals_alive > 0)
		{
			r->dead = 0;
			return ROUND_NOTHING;
		}
		// The last round: not won. The Watchdog restarts its rival (rounds.h)
		if (r->number == ROUND_COUNT - 1)
		{
			if (++r->dead < ROUND_RESTART_FRAMES)
				return ROUND_NOTHING;
			r->dead = 0;
			return ROUND_RESTARTS;
		}
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

int rounds_min_skill(const rounds *r)
{
	return round_min_skills[r->number];
}

int rounds_layout(const rounds *r)
{
	return round_layouts[r->number];
}

// Where the last round is in its lunges: the beat it's on, and how far through the moving half
// of that beat, 0 to 254 (-1 in the still half, or once it's closed all the way, or not closing)
static int lunge(const rounds *r, int *beat)
{
	int within;

	*beat = 0;
	if (r->number != ROUND_COUNT - 1 || r->phase != ROUND_PLAYING)
		return -1;
	*beat = r->frames >> ROUND_LUNGE_SHIFT;
	if (*beat * ROUND_LUNGE_DISTANCE >= ROUND_CLOSE_MAX)
		return -1;
	within = r->frames & ((1 << ROUND_LUNGE_SHIFT) - 1);
	if (within < (1 << (ROUND_LUNGE_SHIFT - 1)))
		return -1; // the still half: it starts with one, so the round opens quietly
	return (within - (1 << (ROUND_LUNGE_SHIFT - 1))) << (9 - ROUND_LUNGE_SHIFT); // 0..254
}

int rounds_closing(const rounds *r)
{
	int beat;
	int u = lunge(r, &beat);
	int closed = beat * ROUND_LUNGE_DISTANCE; // every lunge before this beat's

	if (u >= 0)
		closed += (ROUND_LUNGE_DISTANCE * smooth(u)) >> 8; // part way through this one, eased
	return closed < ROUND_CLOSE_MAX ? closed : ROUND_CLOSE_MAX;
}

int rounds_rumble(const rounds *r)
{
	int beat;
	int u = lunge(r, &beat);

	if (u < 0)
		return 0;
	// The walls' speed, as a bump: u (256 - u) is 0 at either end and 16384 in the middle,
	// >> 6 makes that 256. The same shape as smoothstep's slope
	return (ROUND_RUMBLE_MAX * ((u * (256 - u)) >> 6)) >> 8;
}
