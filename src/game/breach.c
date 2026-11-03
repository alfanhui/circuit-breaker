#include "breach.h"

// A glitched frame's tint (the colour offset): white, pink, cyan, or dark
static const short tints[4][3] = {
	{160, 160, 160},
	{200, -60, 140},
	{-80, 140, 200},
	{-180, -180, -180},
};

// xorshift32, as the flicker's: three shifts and XORs, a long sequence that never reaches 0
static unsigned int next_random(breach *b)
{
	unsigned int x = b->random;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	b->random = x;
	return x;
}

void breach_init(breach *b)
{
	b->phase = BREACH_NONE;
	b->frames = 0;
	b->random = 0x2545f491u; // any non-zero start: the same glitches every time
	b->glitch = (breach_glitch){0};
	b->wants_up = 0;
}

void breach_start(breach *b)
{
	breach_init(b);
	b->phase = BREACH_TEAR;
}

// A glitched frame: the picture's jump (smaller when calm), a tint, and 0 to 3 bands
static void glitch_frame(breach *b, int calm)
{
	breach_glitch *g = &b->glitch;
	unsigned int r = next_random(b);
	const short *tint = tints[(r >> 14) & 3];

	// (r >> 4) & 63 is 0..63: less 32 is -32..31. A calm frame jumps a quarter as far
	g->shift_x = (short)((((int)(r >> 4) & 63) - BREACH_SHIFT_X_MAX) >> (calm ? 2 : 0));
	g->shift_y = (short)((((int)(r >> 10) & 15) - BREACH_SHIFT_Y_MAX) >> (calm ? 2 : 0));
	g->red = tint[0];
	g->green = tint[1];
	g->blue = tint[2];
	g->band_count = (unsigned char)((r >> 16) & 3);
	for (int i = 0; i < g->band_count; ++i)
	{
		unsigned int n = next_random(b);

		// 0..127 + 0..63: anywhere from the top to 190, with room under it for the tallest band
		g->bands[i].top = (short)((n & 127) + ((n >> 8) & 63));
		g->bands[i].height = (short)(2 + ((n >> 16) & 15));
		g->bands[i].pink = (unsigned char)((n >> 20) & 1);
	}
}

// On to the next part once this one's had its frames
static void next_after(breach *b, int frames, int next)
{
	if (b->frames >= frames)
	{
		b->phase = (unsigned char)next;
		b->frames = 0;
	}
}

void breach_update(breach *b)
{
	breach_glitch *g = &b->glitch;

	*g = (breach_glitch){0};
	if (b->phase == BREACH_NONE || b->phase == BREACH_ON_FOOT)
		return;
	// Lying still: counted only as far as it matters (frames is a short: lying there for ever
	// mustn't wrap it round to 0)
	if (b->phase == BREACH_STOPPED)
	{
		if (b->frames < BREACH_LIE_FRAMES)
			b->frames++;
		else if (b->wants_up)
			next_after(b, 0, BREACH_RISE); // pressed sooner: up now
		return;
	}
	b->frames++;
	switch (b->phase)
	{
	case BREACH_TEAR:
	{
		// Worse and worse: a quarter of the frames glitch at first, then half, then three in four
		// (frames >> 5: 0, 1, 2 over the 72 frames). The first frame always does: the hit
		int level = 1 + (b->frames >> 5);
		unsigned int r = next_random(b);

		if (b->frames == 1 || (int)(r & 3) < level)
		{
			glitch_frame(b, 0);
			// Now and then the reset gets the screen for a frame, but it's never let finish
			g->reset_shows = (unsigned char)(b->frames >= 8 && ((r >> 18) & 3) == 0);
		}
		next_after(b, BREACH_TEAR_FRAMES, BREACH_THROUGH);
		break;
	}
	case BREACH_THROUGH:
		// Through the hole: the odd small glitch, dying away
		if ((next_random(b) & 7) == 0)
			glitch_frame(b, 1);
		next_after(b, BREACH_THROUGH_FRAMES, BREACH_FALL);
		break;
	case BREACH_FALL:
		next_after(b, BREACH_FALL_FRAMES, BREACH_SKID);
		break;
	case BREACH_SKID:
		next_after(b, BREACH_SKID_FRAMES, BREACH_STOPPED);
		break;
	case BREACH_RISE:
		next_after(b, BREACH_RISE_FRAMES, BREACH_ON_FOOT);
		break;
	}
}

void breach_get_up(breach *b)
{
	if (b->phase != BREACH_STOPPED)
		return;
	if (b->frames < BREACH_LIE_FRAMES)
		b->wants_up = 1; // kept: up once the frames have passed (breach_update)
	else
		next_after(b, 0, BREACH_RISE);
}

// Getting up, how much of the tilt and drop is left: 256 lying, 0 stood. Eased in and out
// (smoothstep, 3t^2 - 2t^3, in 256ths: t x t x (768 - 2t) >> 16): it starts slowly, as pushing
// yourself up off the floor does, is quickest midway, and settles gently at the top
static int rise_left(const breach *b)
{
	const int t = b->frames << (8 - BREACH_RISE_SHIFT); // 0..256: the share of the rise done

	return 256 - ((t * t * (768 - 2 * t)) >> 16);
}

// The skid's frames still to go: 31 down to 0 (frames counts 0, the frame it began, to 31)
static int skid_left(const breach *b)
{
	return BREACH_SKID_FRAMES - 1 - b->frames;
}

int breach_distance(const breach *b, int cruise)
{
	const int speed = cruise > BREACH_SPEED_MIN ? cruise : BREACH_SPEED_MIN;

	switch (b->phase)
	{
	case BREACH_THROUGH:
	case BREACH_FALL:
		return speed;
	case BREACH_SKID:
		// The share of the speed left: frames to go / 32, as a shift. Straight down to nothing,
		// like brakes locked, so the stop is sudden
		return (speed * skid_left(b)) >> BREACH_SKID_SHIFT;
	default:
		return 0;
	}
}

int breach_landed(const breach *b)
{
	return b->phase == BREACH_SKID && b->frames == 0;
}

// In the skid, from where the fall left it to where it rests: rest + (fall - rest) x the share
// of the skid still to go
static int settle(const breach *b, int fall, int rest)
{
	return rest + (((fall - rest) * skid_left(b)) >> BREACH_SKID_SHIFT);
}

int breach_roll(const breach *b)
{
	switch (b->phase)
	{
	case BREACH_TEAR:
		// A lurch with the picture's jump: up to 512, about 3 degrees
		return (b->glitch.shift_x | b->glitch.shift_y) != 0 ? b->glitch.shift_x * 16 : 0;
	case BREACH_FALL:
		return (BREACH_FALL_ROLL * b->frames) >> 5; // 32 frames: a shift
	case BREACH_SKID:
		return settle(b, BREACH_FALL_ROLL, BREACH_REST_ROLL);
	case BREACH_STOPPED:
		return BREACH_REST_ROLL;
	case BREACH_RISE:
		return (BREACH_REST_ROLL * rise_left(b)) >> 8;
	default:
		return 0;
	}
}

int breach_dip(const breach *b)
{
	switch (b->phase)
	{
	case BREACH_FALL:
		return (BREACH_FALL_DIP * b->frames) >> 5;
	case BREACH_SKID:
		return settle(b, BREACH_FALL_DIP, BREACH_REST_DIP);
	case BREACH_STOPPED:
		return BREACH_REST_DIP;
	case BREACH_RISE:
		return (BREACH_REST_DIP * rise_left(b)) >> 8;
	default:
		return 0;
	}
}

int breach_drop(const breach *b)
{
	switch (b->phase)
	{
	case BREACH_FALL:
		return (BREACH_DROP * b->frames) >> 5;
	case BREACH_SKID:
	case BREACH_STOPPED:
		return BREACH_DROP;
	case BREACH_RISE:
		return (BREACH_DROP * rise_left(b)) >> 8;
	default:
		return 0;
	}
}

int breach_rumble(const breach *b)
{
	return b->phase == BREACH_SKID ? (BREACH_RUMBLE_MAX * skid_left(b)) >> BREACH_SKID_SHIFT : 0;
}

int breach_typed_chars(const breach *b)
{
	const int count = b->frames >> 1;

	if (b->phase != BREACH_TEAR)
		return 0;
	return count < BREACH_RESET_CHARS_MAX ? count : BREACH_RESET_CHARS_MAX;
}
