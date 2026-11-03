#include "wall_flicker.h"

/*
 * Each round's section. Rounds 2 and 3 are a hint you may not notice: 3 pieces, a burst about
 * once a second (1 in 64 steps) then twice a second. Round 4 it grows (4 pieces, 4 a second);
 * round 5 it's obvious: 4 pieces, glitching nearly all the time (1 in 4). It grows in how often,
 * not how wide: a narrow gap you aim for, 3-4 pieces (WALL_FLICKER_PIECES_MAX), not a long run
 * Each is on the far side of the arena from where its round starts you (arena_start): it's
 * something you find, out there, not something in your face as the round begins
 */
typedef struct
{
	unsigned char count;
	unsigned char side;
	short along_min;
	unsigned char chance;
	unsigned char gives_way;
} section;

static const section sections[] = {
	{0, WALL_FLICKER_NORTH, 0, 0, 0},		   // round 1: nothing wrong yet
	{3, WALL_FLICKER_NORTH, 2000, 63, 0},	   // 2000 to 2750 west of the middle
	{3, WALL_FLICKER_EAST, -2250, 31, 0},	   // somewhere else: -2250 to -1500 (north of the middle)
	{4, WALL_FLICKER_NORTH, -3000, 15, 0},	   // grown: -3000 to -2000, the north-east room's (you start
										   // in the south-west one), clear of the rooms' cross
	{4, WALL_FLICKER_WEST, -500, 3, 1}, 		   // obvious: -500 to 500, mid wall, inside the square closed in
};
#define SECTION_COUNT (int)(sizeof(sections) / sizeof(sections[0]))

// xorshift32: three shifts and XORs, a long sequence that never reaches 0
static unsigned int next_random(wall_flicker *f)
{
	unsigned int x = f->random;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	f->random = x;
	return x;
}

void wall_flicker_set_round(wall_flicker *f, int round)
{
	const section *s = &sections[round >= 0 && round < SECTION_COUNT ? round : 0];

	f->count = s->count;
	f->side = s->side;
	f->along_min = s->along_min;
	f->chance = s->chance;
	f->gives_way = s->gives_way;
	f->random = 0x9e3779b9u ^ (unsigned int)(round + 1); // the same glitches every time round
	f->burst = 0;
	f->hidden = 0;
	f->flashed = 0;
}

void wall_flicker_step(wall_flicker *f)
{
	const unsigned short pieces = (unsigned short)((1u << f->count) - 1); // a bit for each piece
	unsigned int a;
	unsigned int b;

	f->hidden = 0;
	f->flashed = 0;
	if (f->count == 0)
		return;
	if (f->burst == 0)
	{
		// Holding: a burst starts now and then
		if ((next_random(f) & f->chance) != 0)
			return;
		f->burst = (unsigned char)(2 + (next_random(f) & 3)); // 2 to 5 steps
	}
	f->burst--;
	// Two random words: a piece is hidden where both bits are set, flashed where only the
	// first is (so never both), each a quarter of the pieces
	a = next_random(f);
	b = next_random(f);
	f->hidden = (unsigned short)(a & b & pieces);
	f->flashed = (unsigned short)(a & ~b & pieces);
	if ((f->hidden | f->flashed) == 0)
		f->flashed = (unsigned short)(1u << ((b >> 16) & 1)); // a burst always shows: the 1st or 2nd piece
}

int wall_flicker_along_max(const wall_flicker *f)
{
	return f->along_min + f->count * WALL_FLICKER_PIECE;
}

int wall_flicker_hit(const wall_flicker *f, world_point at, int heading, int limit)
{
	// North and south walls run along x; east and west along y (north is -y, east is -x)
	const int along = (f->side == WALL_FLICKER_NORTH || f->side == WALL_FLICKER_SOUTH) ? at.x : at.y;
	int against;

	if (f->count == 0 || heading != f->side)
		return 0;
	switch (f->side)
	{
	case WALL_FLICKER_NORTH:
		against = at.y <= -limit;
		break;
	case WALL_FLICKER_EAST:
		against = at.x <= -limit;
		break;
	case WALL_FLICKER_SOUTH:
		against = at.y >= limit;
		break;
	default:
		against = at.x >= limit;
		break;
	}
	return against && along >= f->along_min - WALL_FLICKER_HIT_MARGIN &&
		   along <= wall_flicker_along_max(f) + WALL_FLICKER_HIT_MARGIN;
}

void wall_flicker_open(wall_flicker *f)
{
	f->hidden = (unsigned short)((1u << f->count) - 1);
	f->flashed = 0;
}
