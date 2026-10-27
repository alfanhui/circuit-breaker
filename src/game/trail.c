#include "trail.h"

static int points_equal(world_point a, world_point b)
{
	return a.x == b.x && a.y == b.y;
}

static int live_segment_exists(const trail *t)
{
	return t->corner_count > 0 && !points_equal(t->corners[t->corner_count - 1], t->head);
}

void trail_reset(trail *t, world_point start)
{
	t->corners[0] = start;
	t->corner_count = 1;
	t->version++;
	t->head = start;
}

void trail_set_head(trail *t, world_point head)
{
	t->head = head;
}

void trail_add_corner(trail *t)
{
	int i;

	if (!live_segment_exists(t))
		return;

	if (t->corner_count == TRAIL_MAX_CORNERS)
	{
		// Only happens on a turn, so shifting 64 points is cheaper than ring-buffer bookkeeping every frame
		for (i = 1; i < TRAIL_MAX_CORNERS; ++i)
			t->corners[i - 1] = t->corners[i];
		t->corner_count--;
	}
	t->corners[t->corner_count++] = t->head;
	t->version++;
}

int trail_segment_count(const trail *t)
{
	if (t->corner_count == 0)
		return 0;
	return (t->corner_count - 1) + live_segment_exists(t);
}

// Moves to toward from by up to gap units along their shared axis.
static void pull_back(world_point from, world_point *to, int gap)
{
	int dx = to->x - from.x;
	int dy = to->y - from.y;
	int length = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy); // segments are axis aligned

	if (gap > length)
		gap = length;
	if (dx != 0)
		to->x = (short)(to->x - ((dx > 0) ? gap : -gap));
	else if (dy != 0)
		to->y = (short)(to->y - ((dy > 0) ? gap : -gap));
}

int trail_get_drawn_head(const trail *t, int head_gap, world_point *head)
{
	world_point last;

	if (!live_segment_exists(t))
		return 0;
	last = t->corners[t->corner_count - 1];
	*head = t->head;
	pull_back(last, head, head_gap);
	return !points_equal(last, *head);
}

int trail_get_segment(const trail *t, int index, int head_gap, world_point *from, world_point *to)
{
	if (index < 0 || index >= trail_segment_count(t))
		return 0;

	*from = t->corners[index];
	if (index + 1 < t->corner_count)
	{
		*to = t->corners[index + 1];
		return 1;
	}
	return trail_get_drawn_head(t, head_gap, to);
}
