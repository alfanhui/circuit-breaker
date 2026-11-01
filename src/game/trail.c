#include "trail.h"

static int points_equal(world_point a, world_point b)
{
	return a.x == b.x && a.y == b.y;
}

static int live_segment_exists(const trail *t)
{
	return t->corner_count > 0 && !points_equal(t->corners[t->corner_count - 1], t->head);
}

// Grows the box to take in p: four comparisons, cheap enough for every frame's new head
static void box_add(world_rect *box, world_point p)
{
	if (p.x < box->x_min)
		box->x_min = p.x;
	if (p.x > box->x_max)
		box->x_max = p.x;
	if (p.y < box->y_min)
		box->y_min = p.y;
	if (p.y > box->y_max)
		box->y_max = p.y;
}

static void box_at(world_rect *box, world_point p)
{
	box->x_min = box->x_max = p.x;
	box->y_min = box->y_max = p.y;
}

void trail_reset(trail *t, world_point start)
{
	t->corners[0] = start;
	t->corner_count = 1;
	t->version++;
	t->head = start;
	box_at(&t->box, start);
}

void trail_shift(trail *t, int dx, int dy)
{
	for (int i = 0; i < t->corner_count; ++i)
	{
		t->corners[i].x = (short)(t->corners[i].x + dx);
		t->corners[i].y = (short)(t->corners[i].y + dy);
	}
	t->head.x = (short)(t->head.x + dx);
	t->head.y = (short)(t->head.y + dy);
	t->box.x_min = (short)(t->box.x_min + dx);
	t->box.x_max = (short)(t->box.x_max + dx);
	t->box.y_min = (short)(t->box.y_min + dy);
	t->box.y_max = (short)(t->box.y_max + dy);
	t->version++; // the corners moved: a renderer's cached copy is stale
}

void trail_set_head(trail *t, world_point head)
{
	t->head = head;
	box_add(&t->box, head);
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
		// The oldest corner's gone: the box may shrink. Built again from what's left (on a turn
		// only, as the shift above)
		box_at(&t->box, t->head);
		for (i = 0; i < t->corner_count; ++i)
			box_add(&t->box, t->corners[i]);
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
