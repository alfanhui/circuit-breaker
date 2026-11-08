#include "test.h"
#include "../src/game/trail.h"

static world_point pt(short x, short y)
{
	world_point p;
	p.x = x;
	p.y = y;
	return p;
}

static void reset_starts_with_no_segments(void)
{
	trail t;
	trail_reset(&t, pt(10, 20));
	CHECK_EQ(1, t.corner_count);
	CHECK_EQ(0, trail_segment_count(&t));
}

static void live_segment_grows_from_last_corner_to_head(void)
{
	trail t;
	world_point from, to;

	trail_reset(&t, pt(0, 0));
	trail_set_head(&t, pt(0, -50));
	CHECK_EQ(1, trail_segment_count(&t));
	CHECK(trail_get_segment(&t, 0, 0, &from, &to));
	CHECK_EQ(0, from.y);
	CHECK_EQ(-50, to.y);

	trail_set_head(&t, pt(0, -120));
	CHECK(trail_get_segment(&t, 0, 0, &from, &to));
	CHECK_EQ(-120, to.y);
}

static void turn_pins_corner_and_starts_new_segment(void)
{
	trail t;
	world_point from, to;

	trail_reset(&t, pt(0, 0));
	trail_set_head(&t, pt(0, -100));
	trail_add_corner(&t);
	CHECK_EQ(2, t.corner_count);
	CHECK_EQ(1, trail_segment_count(&t)); // live segment has no length yet

	trail_set_head(&t, pt(-40, -100));
	CHECK_EQ(2, trail_segment_count(&t));
	CHECK(trail_get_segment(&t, 0, 0, &from, &to));
	CHECK_EQ(0, from.y);
	CHECK_EQ(-100, to.y);
	CHECK(trail_get_segment(&t, 1, 0, &from, &to));
	CHECK_EQ(0, from.x);
	CHECK_EQ(-100, from.y);
	CHECK_EQ(-40, to.x);
	CHECK_EQ(-100, to.y);
}

static void segments_are_continuous(void)
{
	trail t;
	world_point from, to;
	world_point previous_to = {0, 0};
	int i;

	trail_reset(&t, pt(0, 0));
	trail_set_head(&t, pt(0, -100));
	trail_add_corner(&t);
	trail_set_head(&t, pt(-60, -100));
	trail_add_corner(&t);
	trail_set_head(&t, pt(-60, 30));

	CHECK_EQ(3, trail_segment_count(&t));
	for (i = 0; i < trail_segment_count(&t); ++i)
	{
		CHECK(trail_get_segment(&t, i, 0, &from, &to));
		if (i > 0)
		{
			CHECK_EQ(previous_to.x, from.x);
			CHECK_EQ(previous_to.y, from.y);
		}
		previous_to = to;
	}
	CHECK_EQ(-60, previous_to.x);
	CHECK_EQ(30, previous_to.y);
}

static void corner_ignored_when_head_has_not_moved(void)
{
	trail t;
	trail_reset(&t, pt(5, 5));
	trail_add_corner(&t);
	trail_add_corner(&t);
	CHECK_EQ(1, t.corner_count);
}

static void head_gap_shortens_only_the_live_segment(void)
{
	trail t;
	world_point from, to;

	trail_reset(&t, pt(0, 0));
	trail_set_head(&t, pt(0, -100));
	trail_add_corner(&t);
	trail_set_head(&t, pt(80, -100));

	CHECK(trail_get_segment(&t, 0, 25, &from, &to));
	CHECK_EQ(-100, to.y); // committed segment untouched
	CHECK(trail_get_segment(&t, 1, 25, &from, &to));
	CHECK_EQ(55, to.x);
	CHECK_EQ(-100, to.y);
}

static void head_gap_longer_than_segment_hides_it(void)
{
	trail t;
	world_point from, to;

	trail_reset(&t, pt(0, 0));
	trail_set_head(&t, pt(0, 10));
	CHECK(!trail_get_segment(&t, 0, 25, &from, &to));
}

static void out_of_range_index_rejected(void)
{
	trail t;
	world_point from, to;

	trail_reset(&t, pt(0, 0));
	trail_set_head(&t, pt(0, 10));
	CHECK(!trail_get_segment(&t, -1, 0, &from, &to));
	CHECK(!trail_get_segment(&t, 1, 0, &from, &to));
}

static void full_trail_drops_oldest_corner(void)
{
	trail t;
	world_point from, to;
	int i;

	trail_reset(&t, pt(0, 0));
	// zig-zag so every corner is distinct
	for (i = 1; i <= TRAIL_MAX_CORNERS + 5; ++i)
	{
		trail_set_head(&t, pt((short)(i * 10), (short)((i & 1) * 10)));
		trail_add_corner(&t);
	}
	CHECK_EQ(TRAIL_MAX_CORNERS, t.corner_count);
	CHECK(trail_segment_count(&t) <= TRAIL_MAX_SEGMENTS);

	// newest corner kept, oldest ones gone
	CHECK_EQ((TRAIL_MAX_CORNERS + 5) * 10, t.corners[TRAIL_MAX_CORNERS - 1].x);
	CHECK_EQ(6 * 10, t.corners[0].x);

	trail_set_head(&t, pt(5000, 10));
	CHECK_EQ(TRAIL_MAX_SEGMENTS, trail_segment_count(&t));
	CHECK(trail_get_segment(&t, TRAIL_MAX_SEGMENTS - 1, 0, &from, &to));
	CHECK_EQ(5000, to.x);
}

static void version_changes_only_when_corners_change(void)
{
	trail t;
	unsigned short version;

	trail_reset(&t, pt(0, 0));
	version = t.version;
	trail_set_head(&t, pt(0, -40));
	CHECK_EQ(version, t.version);

	trail_add_corner(&t);
	CHECK(t.version != version);
	version = t.version;

	trail_add_corner(&t); // head has not moved: ignored
	CHECK_EQ(version, t.version);

	trail_reset(&t, pt(0, 0));
	CHECK(t.version != version);
}

static void version_changes_when_full_trail_drops_a_corner(void)
{
	trail t;
	unsigned short version;
	int i;

	trail_reset(&t, pt(0, 0));
	for (i = 1; i < TRAIL_MAX_CORNERS; ++i)
	{
		trail_set_head(&t, pt((short)(i * 10), (short)((i & 1) * 10)));
		trail_add_corner(&t);
	}
	version = t.version;
	trail_set_head(&t, pt(5000, 0));
	trail_add_corner(&t);
	CHECK_EQ(TRAIL_MAX_CORNERS, t.corner_count);
	CHECK(t.version != version);
}

static void drawn_head_matches_live_segment_end(void)
{
	trail t;
	world_point from, to, head;

	trail_reset(&t, pt(0, 0));
	CHECK(!trail_get_drawn_head(&t, 0, &head));

	trail_set_head(&t, pt(0, -100));
	CHECK(trail_get_drawn_head(&t, 25, &head));
	CHECK(trail_get_segment(&t, 0, 25, &from, &to));
	CHECK_EQ(to.x, head.x);
	CHECK_EQ(to.y, head.y);
	CHECK_EQ(-75, head.y);

	trail_set_head(&t, pt(0, -20));
	CHECK(!trail_get_drawn_head(&t, 25, &head));
}

// The loop's break slides the world (input.c): the wall keeps its shape, just somewhere else
static void shift_moves_every_corner_and_the_head(void)
{
	trail t;
	world_point p = {100, -200};
	unsigned short version;

	trail_reset(&t, p);
	p.x = 400;
	trail_set_head(&t, p);
	trail_add_corner(&t);
	p.y = 50;
	trail_set_head(&t, p);
	version = t.version;
	trail_shift(&t, -1000, 2000);
	CHECK_EQ(2, t.corner_count);
	CHECK_EQ(-900, t.corners[0].x);
	CHECK_EQ(1800, t.corners[0].y);
	CHECK_EQ(-600, t.corners[1].x);
	CHECK_EQ(1800, t.corners[1].y);
	CHECK_EQ(-600, t.head.x);
	CHECK_EQ(2050, t.head.y);
	CHECK(t.version != version);
}

// Whether every corner and the head are inside the trail's box (and so whether the whole-trail
// skip in collision.c can trust it)
static int box_holds_all(const trail *t)
{
	int ok = t->head.x >= t->box.x_min && t->head.x <= t->box.x_max && t->head.y >= t->box.y_min &&
			 t->head.y <= t->box.y_max;

	for (int i = 0; i < t->corner_count; ++i)
		ok &= t->corners[i].x >= t->box.x_min && t->corners[i].x <= t->box.x_max && t->corners[i].y >= t->box.y_min &&
			  t->corners[i].y <= t->box.y_max;
	return ok;
}

// A spiral outwards, 100 turns: past 64 the oldest (innermost) drop off and the box shrinks to
// what's left, exactly. Then shifted with the trail
static void box_holds_every_corner_and_the_head(void)
{
	trail t;
	world_point p = pt(0, 0);
	int ok = 1;

	trail_reset(&t, p);
	ok &= box_holds_all(&t);
	for (int i = 1; i <= 100; ++i)
	{
		int step = (short)(i * 10);

		if (i & 1)
			p.x = (short)(p.x + ((i & 2) ? -step : step));
		else
			p.y = (short)(p.y + ((i & 2) ? -step : step));
		trail_set_head(&t, p);
		ok &= box_holds_all(&t);
		trail_add_corner(&t);
		ok &= box_holds_all(&t);
	}
	CHECK(ok);
	// Exactly the remaining corners' box once corners have dropped (a turn rebuilds it)
	{
		short x_min = t.head.x, x_max = t.head.x, y_min = t.head.y, y_max = t.head.y;

		for (int i = 0; i < t.corner_count; ++i)
		{
			x_min = t.corners[i].x < x_min ? t.corners[i].x : x_min;
			x_max = t.corners[i].x > x_max ? t.corners[i].x : x_max;
			y_min = t.corners[i].y < y_min ? t.corners[i].y : y_min;
			y_max = t.corners[i].y > y_max ? t.corners[i].y : y_max;
		}
		CHECK_EQ(x_min, t.box.x_min);
		CHECK_EQ(x_max, t.box.x_max);
		CHECK_EQ(y_min, t.box.y_min);
		CHECK_EQ(y_max, t.box.y_max);
	}
	trail_shift(&t, 1000, -2000);
	CHECK(box_holds_all(&t));
}

void run_trail_tests(void)
{
	RUN(box_holds_every_corner_and_the_head);
	RUN(version_changes_only_when_corners_change);
	RUN(version_changes_when_full_trail_drops_a_corner);
	RUN(drawn_head_matches_live_segment_end);
	RUN(reset_starts_with_no_segments);
	RUN(live_segment_grows_from_last_corner_to_head);
	RUN(turn_pins_corner_and_starts_new_segment);
	RUN(segments_are_continuous);
	RUN(corner_ignored_when_head_has_not_moved);
	RUN(head_gap_shortens_only_the_live_segment);
	RUN(head_gap_longer_than_segment_hides_it);
	RUN(out_of_range_index_rejected);
	RUN(full_trail_drops_oldest_corner);
	RUN(shift_moves_every_corner_and_the_head);
}
