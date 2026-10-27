#include "test.h"
#include "../src/game/view_cull.h"

static world_point pt(int x, int y)
{
	world_point p;
	p.x = (short)x;
	p.y = (short)y;
	return p;
}

static int inside(world_rect r, world_point p)
{
	return p.x >= r.x_min && p.x <= r.x_max && p.y >= r.y_min && p.y <= r.y_max;
}

static void first_person_sees_ahead_not_behind(void)
{
	world_rect r = view_cull_rect(pt(0, 0), HEADING_NORTH, VIEW_CULL_FIRST_PERSON, WORLD_VIEW_RANGE); // north is -y

	CHECK(inside(r, pt(0, -5000)));							 // ahead
	CHECK(inside(r, pt(-5000, -5000)));						 // 45 degrees to the side: edge of a 90 degree view
	CHECK(!inside(r, pt(0, 1000)));							 // behind
	CHECK(inside(r, pt(0, VIEW_CULL_BEHIND)));				 // the margin for pieces straddling the camera
	CHECK_EQ(-WORLD_VIEW_RANGE, r.y_min);
}

static void first_person_follows_the_heading(void)
{
	world_rect east = view_cull_rect(pt(100, 200), HEADING_EAST, VIEW_CULL_FIRST_PERSON, WORLD_VIEW_RANGE); // east is -x
	world_rect west = view_cull_rect(pt(100, 200), HEADING_WEST, VIEW_CULL_FIRST_PERSON, WORLD_VIEW_RANGE);

	CHECK(inside(east, pt(100 - 4000, 200)));
	CHECK(!inside(east, pt(100 + 1000, 200)));
	CHECK(inside(west, pt(100 + 4000, 200)));
	CHECK(!inside(west, pt(100 - 1000, 200)));
}

static void turning_sees_all_round(void)
{
	world_rect r = view_cull_rect(pt(0, 0), HEADING_NORTH, VIEW_CULL_TURNING, WORLD_VIEW_RANGE);
	CHECK(inside(r, pt(0, 5000))); // behind is in: the camera may still face the old way
	CHECK(inside(r, pt(0, -5000)));
}

static void iso_sees_only_near_the_cycle(void)
{
	world_rect r = view_cull_rect(pt(1000, 1000), HEADING_NORTH, VIEW_CULL_ISO, WORLD_VIEW_RANGE);
	CHECK(inside(r, pt(1000 + VIEW_CULL_ISO_RANGE, 1000)));
	CHECK(!inside(r, pt(1000 + VIEW_CULL_ISO_RANGE + 1, 1000)));
	CHECK(!inside(r, pt(1000, 1000 - 5000)));
}

static void wall_range_is_clamped_to_the_map(void)
{
	// An eye near the west wall of the largest arena: +28000 would wrap a short to the far side
	world_rect r = view_cull_rect(pt(13900, 0), HEADING_WEST, VIEW_CULL_FIRST_PERSON, WORLD_WALL_VIEW_RANGE);
	CHECK_EQ(WORLD_COORD_LIMIT, r.x_max);
	CHECK(r.x_min < 13900); // still includes just behind the camera
	CHECK_EQ(-WORLD_WALL_VIEW_RANGE, r.y_min); // sideways from y = 0: inside the limit, unclamped
}

void run_view_cull_tests(void)
{
	RUN(first_person_sees_ahead_not_behind);
	RUN(first_person_follows_the_heading);
	RUN(turning_sees_all_round);
	RUN(iso_sees_only_near_the_cycle);
	RUN(wall_range_is_clamped_to_the_map);
}
