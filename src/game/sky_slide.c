#include "sky_slide.h"
#include "world.h"

void sky_slide_init(sky_slide *s)
{
	s->x = 0;
	s->y = 0;
}

void sky_slide_add(sky_slide *s, int dx, int dy)
{
	s->x += dx;
	s->y += dy;
}

// One axis a step nearer 0, never past it
static int ease_to_zero(int v)
{
	return v > SKY_SLIDE_EASE ? v - SKY_SLIDE_EASE : v < -SKY_SLIDE_EASE ? v + SKY_SLIDE_EASE : 0;
}

void sky_slide_step(sky_slide *s)
{
	s->x = ease_to_zero(s->x);
	s->y = ease_to_zero(s->y);
}

void sky_slide_frame(const sky_slide *s, int side, int *out, int *across)
{
	/*
	 * corridor_point_behind's map, backwards. A wall to the north or south (its corridor runs along
	 * y): across is x itself, and out is y the way the side's heading steps (north is -y). East or
	 * west: across is y, and out is x the way the heading steps (east is -x: world.h)
	 */
	if (side == HEADING_NORTH || side == HEADING_SOUTH)
	{
		*across = s->x;
		*out = WORLD_HEADING_DY(side) * s->y;
	}
	else
	{
		*across = s->y;
		*out = WORLD_HEADING_DX(side) * s->x;
	}
}
