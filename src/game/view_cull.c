#include "view_cull.h"

// Rectangle corners are shorts: keep them inside +-WORLD_COORD_LIMIT (an eye near one wall plus
// the wall range would otherwise wrap round to the far side of the map)
static short clamp_coordinate(int v)
{
	if (v > WORLD_COORD_LIMIT)
		return WORLD_COORD_LIMIT;
	if (v < -WORLD_COORD_LIMIT)
		return -WORLD_COORD_LIMIT;
	return (short)v;
}

static world_rect around(world_point centre, int range)
{
	world_rect r;
	r.x_min = clamp_coordinate(centre.x - range);
	r.x_max = clamp_coordinate(centre.x + range);
	r.y_min = clamp_coordinate(centre.y - range);
	r.y_max = clamp_coordinate(centre.y + range);
	return r;
}

world_rect view_cull_rect(world_point eye, int heading, view_cull_mode mode, int range)
{
	world_rect r;
	int dx = WORLD_HEADING_DX(heading);
	int dy = WORLD_HEADING_DY(heading);
	int a, b;

	if (mode == VIEW_CULL_ISO)
		return around(eye, VIEW_CULL_ISO_RANGE);
	if (mode != VIEW_CULL_FIRST_PERSON)
		return around(eye, range);

	// Start from the full square, then cut the forward axis to [behind, range] in front
	r = around(eye, range);
	if (dx != 0)
	{
		a = eye.x - dx * VIEW_CULL_BEHIND;
		b = eye.x + dx * range;
		r.x_min = clamp_coordinate(a < b ? a : b);
		r.x_max = clamp_coordinate(a < b ? b : a);
	}
	else
	{
		a = eye.y - dy * VIEW_CULL_BEHIND;
		b = eye.y + dy * range;
		r.y_min = clamp_coordinate(a < b ? a : b);
		r.y_max = clamp_coordinate(a < b ? b : a);
	}
	return r;
}
