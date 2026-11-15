#include "floor_fade.h"

#define MAX_LEVEL (FLOOR_FADE_LEVELS - 1)

static int level_for_distance(const floor_fade_view *view, int distance)
{
	if (distance <= view->fade_start)
		return 0;
	if (distance >= view->fade_end)
		return MAX_LEVEL;
	return (distance - view->fade_start) * MAX_LEVEL / (view->fade_end - view->fade_start);
}

int floor_fade_ratio(int level)
{
	if (level <= 0)
		return 0;
	if (level >= MAX_LEVEL)
		return FLOOR_FADE_RATIO_MAX;
	// Rounded to the nearest 32nd. Only runs when the view changes (once per screen row), not per frame
	return (level * FLOOR_FADE_RATIO_MAX + MAX_LEVEL / 2) / MAX_LEVEL;
}

void floor_fade_tables_init(floor_fade_tables *t)
{
	for (int i = 0; i < FLOOR_FADE_TABLES; ++i)
		t->built[i] = 0;
	t->showing = -1;
}

int floor_fade_tables_build(floor_fade_tables *t, unsigned int tag)
{
	int pick = -1;

	for (int i = 0; i < FLOOR_FADE_TABLES; ++i)
		if (t->built[i] && t->tag[i] == tag)
			return i; // built again in the same frame: the same table
	// Else one never built, or the oldest, but never the one on screen
	for (int i = 0; i < FLOOR_FADE_TABLES; ++i)
	{
		if (i == t->showing)
			continue;
		if (!t->built[i])
		{
			pick = i;
			break;
		}
		if (pick < 0 || (int)(t->tag[i] - t->tag[pick]) < 0)
			pick = i;
	}
	t->built[pick] = 1;
	t->tag[pick] = tag;
	return pick;
}

int floor_fade_tables_show(floor_fade_tables *t, unsigned int shown)
{
	int best = -1;

	for (int i = 0; i < FLOOR_FADE_TABLES; ++i)
		// Built for that frame or one before it; of those, the newest
		if (t->built[i] && (int)(shown - t->tag[i]) >= 0 && (best < 0 || (int)(t->tag[i] - t->tag[best]) > 0))
			best = i;
	if (best >= 0)
		t->showing = best;
	return t->showing;
}

void floor_fade_rows(const floor_fade_view *view, unsigned char *levels, int rows)
{
	for (int row = 0; row < rows; ++row)
	{
		int dy = row - view->centre_row; // pixels below the centre (negative: above)
		// distance = eye * (focal - tilt * dy) / (tilt * focal + dy), everything x1000 for tilt.
		// The ratio first (x256), then the eye's height: eye x num in one go is past 32 bits
		// for a high camera (3804 x 597000 for the spectator's), and wrapped negative. num stays
		// under ~800000 for focal <= 600 and tilt <= 1732 (60 degrees down), so x256 fits
		int num = view->focal * 1000 - view->tilt * dy;
		int den = view->tilt * view->focal + 1000 * dy; // = 1000 * (rows below the horizon)
		int level;

		if (den > 0 && num > 0)
		{
			int ratio = (num << 8) / den; // floor distance per unit of eye height, x256
			// Just under the horizon den is tiny and the ratio huge: over 1024 eye heights away is
			// full fog in any view (and eye x ratio would pass 32 bits)
			level = ratio >= (1 << 18) ? MAX_LEVEL : level_for_distance(view, (view->eye_height * ratio) >> 8);
		}
		else if (den > 0)
			level = 0; // num <= 0: under the camera
		else
		{
			int above = -den / 1000; // rows above the horizon
			level = above < view->glow_rows ? MAX_LEVEL * (view->glow_rows - above) / view->glow_rows : 0;
		}
		levels[row] = (unsigned char)level;
	}
}
