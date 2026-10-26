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

void run_trail_tests(void)
{
	RUN(reset_starts_with_no_segments);
	RUN(live_segment_grows_from_last_corner_to_head);
	RUN(turn_pins_corner_and_starts_new_segment);
	RUN(segments_are_continuous);
	RUN(corner_ignored_when_head_has_not_moved);
	RUN(head_gap_shortens_only_the_live_segment);
	RUN(head_gap_longer_than_segment_hides_it);
	RUN(out_of_range_index_rejected);
	RUN(full_trail_drops_oldest_corner);
}
