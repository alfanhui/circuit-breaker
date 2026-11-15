#include "drone.h"
#include "world.h" // WORLD_HEADING_DX, WORLD_HEADING_DY

void drone_init(drone *d, const drone_stop *route, int stops)
{
	d->route = route;
	d->stops = (unsigned char)stops;
	d->at = 0;
	d->waited = 0;
	d->x = route[0].x;
	d->y = route[0].y;
	d->z = route[0].z;
	d->yaw = (unsigned short)(route[0].heading << 14);
	d->scan = 0;
	d->seen = false;
}

bool drone_waiting(const drone *d)
{
	const drone_stop *s = &d->route[d->at];

	return d->x == s->x && d->y == s->y && d->z == s->z;
}

int drone_heading(const drone *d)
{
	return d->route[d->at].heading;
}

// One axis a step nearer `to`: DRONE_SPEED, or what's left if that's less
static short step_to(short from, short to)
{
	if (from < to)
		return (short)(to - from > DRONE_SPEED ? from + DRONE_SPEED : to);
	if (from > to)
		return (short)(from - to > DRONE_SPEED ? from - DRONE_SPEED : to);
	return from;
}

void drone_update(drone *d)
{
	const drone_stop *s = &d->route[d->at];
	// What's left of the turn, the short way round: the 16-bit difference, read as signed
	const int left = (short)(unsigned short)((s->heading << 14) - d->yaw);
	int turn = left >> DRONE_TURN_SHIFT;

	// The scan sweeps out, then flies back (a power of two: a mask, not a remainder)
	d->scan = (unsigned char)((d->scan + 1) & (DRONE_SCAN_FRAMES - 1));
	if (turn == 0 && left != 0)
		turn = left > 0 ? 1 : -1; // the last few steps of the ease: or it'd never quite get there
	d->yaw = (unsigned short)(d->yaw + turn);

	if (drone_waiting(d))
	{
		if (++d->waited >= s->wait)
		{
			d->waited = 0;
			if (++d->at >= d->stops)
				d->at = 0;
		}
		return;
	}
	d->x = step_to(d->x, s->x);
	d->y = step_to(d->y, s->y);
	d->z = step_to(d->z, s->z);
}

void drone_sensor(const drone *d, int *x, int *y, int *z)
{
	const int heading = drone_heading(d);

	*x = d->x + WORLD_HEADING_DX(heading) * DRONE_NOSE;
	*y = d->y + WORLD_HEADING_DY(heading) * DRONE_NOSE;
	*z = d->z - DRONE_SENSOR_DOWN;
}

int drone_scan_ahead(const drone *d)
{
	// Out from NEAR to RANGE over the sweep: one multiply and a shift (a power of two of frames)
	return DRONE_SCAN_NEAR + (((DRONE_SCAN_RANGE - DRONE_SCAN_NEAR) * d->scan) >> DRONE_SCAN_SHIFT);
}

void drone_scan_rect(const drone *d, drone_box *rect)
{
	const int heading = drone_heading(d);
	const int dx = WORLD_HEADING_DX(heading), dy = WORLD_HEADING_DY(heading);
	const int near = drone_scan_ahead(d), far = near + DRONE_SCAN_DEEP;
	int sx, sy, sz;

	drone_sensor(d, &sx, &sy, &sz);
	// Along where it faces, from near to far; across, as wide as the fan there (45 degrees: far)
	if (dx != 0)
	{
		const int a = sx + dx * near, b = sx + dx * far;

		rect->x_min = (short)(a < b ? a : b);
		rect->x_max = (short)(a < b ? b : a);
		rect->y_min = (short)(sy - far);
		rect->y_max = (short)(sy + far);
	}
	else
	{
		const int a = sy + dy * near, b = sy + dy * far;

		rect->y_min = (short)(a < b ? a : b);
		rect->y_max = (short)(a < b ? b : a);
		rect->x_min = (short)(sx - far);
		rect->x_max = (short)(sx + far);
	}
	rect->top = 0;
}

/*
 * Nothing in the way from (sx, sy, sz) to (ex, ey, ez): tested at 16 points between (the ends left
 * out). Each point's (start + the whole line x k / 16): a shift, no division. A box is in the way
 * if a point's inside it, under its roof. Only boxes the line's bounds touch, and whose roof's over
 * the line's lower end, are tested at all
 */
static bool line_clear(int sx, int sy, int sz, int ex, int ey, int ez, const drone_box *boxes, int box_count)
{
	const int dx = ex - sx, dy = ey - sy, dz = ez - sz;
	const int x_min = dx < 0 ? ex : sx, x_max = dx < 0 ? sx : ex;
	const int y_min = dy < 0 ? ey : sy, y_max = dy < 0 ? sy : ey;
	const int z_low = dz < 0 ? ez : sz;

	for (int b = 0; b < box_count; ++b)
	{
		const drone_box *box = &boxes[b];

		if (box->x_max < x_min || box->x_min > x_max || box->y_max < y_min || box->y_min > y_max || box->top <= z_low)
			continue;
		for (int k = 1; k < (1 << DRONE_SIGHT_STEPS_SHIFT); ++k)
		{
			const int px = sx + ((dx * k) >> DRONE_SIGHT_STEPS_SHIFT);
			const int py = sy + ((dy * k) >> DRONE_SIGHT_STEPS_SHIFT);
			const int pz = sz + ((dz * k) >> DRONE_SIGHT_STEPS_SHIFT);

			if (pz < box->top && px >= box->x_min && px <= box->x_max && py >= box->y_min && py <= box->y_max)
				return false;
		}
	}
	return true;
}

bool drone_sees(const drone *d, int x, int y, int z, const drone_box *boxes, int box_count)
{
	drone_box line;
	int sx, sy, sz;

	drone_scan_rect(d, &line);
	if (x < line.x_min || x > line.x_max || y < line.y_min || y > line.y_max)
		return false; // the scan line isn't under you
	drone_sensor(d, &sx, &sy, &sz);
	return line_clear(sx, sy, sz, x, y, z, boxes, box_count);
}

int drone_scan_lit(const drone *d, const drone_box *boxes, int box_count, drone_box *lit)
{
	drone_box line;
	int sx, sy, sz, count = 0;

	drone_scan_rect(d, &line);
	drone_sensor(d, &sx, &sy, &sz);
	for (int b = 0; b < box_count && count < DRONE_LIT_MAX; ++b)
	{
		const drone_box *box = &boxes[b];
		drone_box part;

		if (box->x_max < line.x_min || box->x_min > line.x_max || box->y_max < line.y_min || box->y_min > line.y_max ||
			box->top >= sz)
			continue; // not under the line, or as high as the sensor (it can't light its own floor)
		part.x_min = box->x_min > line.x_min ? box->x_min : line.x_min;
		part.x_max = box->x_max < line.x_max ? box->x_max : line.x_max;
		part.y_min = box->y_min > line.y_min ? box->y_min : line.y_min;
		part.y_max = box->y_max < line.y_max ? box->y_max : line.y_max;
		part.top = box->top;
		// In a nearer tower's shadow: the sensor can't see the middle of the part, just over it
		if (!line_clear(sx, sy, sz, (part.x_min + part.x_max) >> 1, (part.y_min + part.y_max) >> 1, part.top + 1, boxes,
						box_count))
			continue;
		lit[count++] = part;
	}
	return count;
}

int drone_scan_ground(const drone *d, const drone_box *boxes, int box_count, int none)
{
	drone_box line;
	int x, y;

	drone_scan_rect(d, &line);
	x = (line.x_min + line.x_max) >> 1;
	y = (line.y_min + line.y_max) >> 1;
	for (int b = 0; b < box_count; ++b)
		if (x >= boxes[b].x_min && x <= boxes[b].x_max && y >= boxes[b].y_min && y <= boxes[b].y_max)
			return boxes[b].top;
	return none;
}
