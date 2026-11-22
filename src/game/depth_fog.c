#include "depth_fog.h"

#define MAX_LEVEL (DEPTH_FOG_LEVELS - 1)
#define GOURAUD_NONE 16 // a Gouraud channel that leaves the colour alone
#define FORWARD_ONE 1024 // the forward direction's scale

void depth_fog_set_distances(depth_fog *f, int fade_start, int fade_end)
{
	int span = fade_end - fade_start;

	// The floor (floor_fade.c) gives distance d the level (d - start) * 15 / span, rounded down.
	// So level k begins at the first whole distance where that reaches k:
	// start + k * span / 15, rounded up. Then a wall and the floor it stands on always agree
	for (int k = 1; k <= MAX_LEVEL; ++k)
		f->thresholds[k - 1] = (fade_start + (k * span + MAX_LEVEL - 1) / MAX_LEVEL) * FORWARD_ONE;
}

void depth_fog_set_camera(depth_fog *f, world_point eye, int forward_x, int forward_y)
{
	f->eye = eye;
	f->forward_x = forward_x;
	f->forward_y = forward_y;
}

int depth_fog_level(const depth_fog *f, int x, int y)
{
	int depth = (x - f->eye.x) * f->forward_x + (y - f->eye.y) * f->forward_y;
	int low = 0;
	int high = MAX_LEVEL;

	// Binary search: how many thresholds the depth has reached. Behind the camera (negative
	// depth) reaches none: level 0. 15 thresholds take 4 steps; >> 1 halves without dividing
	while (low < high)
	{
		int middle = (low + high) >> 1;
		if (depth >= f->thresholds[middle])
			low = middle + 1;
		else
			high = middle;
	}
	return low;
}

int depth_fog_box_full(const depth_fog *f, int x_min, int x_max, int y_min, int y_max)
{
	// Depth grows with x if forward's x is positive: then the low end of x is the nearer, else
	// the high end. The same for y. That corner's the least deep of the four
	return depth_fog_level(f, f->forward_x > 0 ? x_min : x_max, f->forward_y > 0 ? y_min : y_max) >= MAX_LEVEL;
}

unsigned short depth_fog_darken(unsigned short colour, int steps)
{
	int r = (colour & 31) - steps, g = ((colour >> 5) & 31) - steps, b = ((colour >> 10) & 31) - steps;

	r = r < 0 ? 0 : r;
	g = g < 0 ? 0 : g;
	b = b < 0 ? 0 : b;
	return (unsigned short)(0x8000 | b << 10 | g << 5 | r);
}

unsigned short depth_fog_gouraud_colour(int level)
{
	int channel;

	if (level < 0)
		level = 0;
	if (level > MAX_LEVEL)
		level = MAX_LEVEL;
	// 16 (no change) at level 0 down to 0 (16 darker) at full fog, rounded to the nearest step
	channel = GOURAUD_NONE - (level * GOURAUD_NONE + MAX_LEVEL / 2) / MAX_LEVEL;
	return (unsigned short)(0x8000 | channel << 10 | channel << 5 | channel);
}

int depth_fog_shared_table(int level0, int level1, int level2, int level3)
{
	// A level is 0 .. 15: a x 16 is a shift by 4 (two of the SH-2's shifts by 2)
	if (level0 == level1 && level2 == level3)
		return (level0 << 4) | level2; // a a b b
	if (level0 == level3 && level1 == level2)
		return DEPTH_FOG_LEVELS * DEPTH_FOG_LEVELS + ((level0 << 4) | level1); // a b b a
	return -1;
}

void depth_fog_shared_levels(int table, int levels[4])
{
	const int second = table >= DEPTH_FOG_LEVELS * DEPTH_FOG_LEVELS; // a b b a
	const int pair = second ? table - DEPTH_FOG_LEVELS * DEPTH_FOG_LEVELS : table;
	const int a = pair >> 4, b = pair & (DEPTH_FOG_LEVELS - 1);

	levels[0] = a;
	levels[1] = second ? b : a;
	levels[2] = b;
	levels[3] = second ? a : b;
}
