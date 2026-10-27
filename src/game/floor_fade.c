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

void floor_fade_rows(const floor_fade_view *view, unsigned char *levels, int rows)
{
	for (int row = 0; row < rows; ++row)
	{
		int dy = row - view->centre_row; // pixels below the centre (negative: above)
		// distance = eye * (focal - tilt * dy) / (tilt * focal + dy), everything x1000 for tilt
		int num = view->eye_height * (view->focal * 1000 - view->tilt * dy);
		int den = view->tilt * view->focal + 1000 * dy; // = 1000 * (rows below the horizon)
		int level;

		if (den > 0)
			level = num > 0 ? level_for_distance(view, num / den) : 0; // num <= 0: under the camera
		else
		{
			int above = -den / 1000; // rows above the horizon
			level = above < view->glow_rows ? MAX_LEVEL * (view->glow_rows - above) / view->glow_rows : 0;
		}
		levels[row] = (unsigned char)level;
	}
}
