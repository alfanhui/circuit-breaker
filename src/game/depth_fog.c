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
