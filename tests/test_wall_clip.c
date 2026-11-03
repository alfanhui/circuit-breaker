#include "test.h"
#include "../src/game/wall_clip.h"

#define LOW_MIN 40	 // the low strip's minimum distance in front (as level.c)
#define HIGH_MIN 240 // the high strip's

// The north wall (y = -8000) as columns every 250, from x = from_x towards -x (east)
#define COLUMNS 49
static world_point columns[COLUMNS];

static void north_wall_columns(int from_x)
{
	for (int k = 0; k < COLUMNS; ++k)
	{
		columns[k].x = (short)(from_x - 250 * k);
		columns[k].y = -8000;
	}
}

static wall_clip_camera camera(int x, int y, int forward_x, int forward_y)
{
	wall_clip_camera c;
	c.eye.x = (short)x;
	c.eye.y = (short)y;
	c.forward_x = forward_x;
	c.forward_y = forward_y;
	c.view_width = WALL_CLIP_VIEW_WIDTH_90;
	c.side_extra16 = 0;
	return c;
}

// The boost widens the view: a piece just off the side of a 90 degree view comes into it
static void wider_view_keeps_pieces_at_the_side(void)
{
	// Facing north from the origin; the piece is 1000 ahead and 1200..1450 to the right (east, -x)
	world_point piece[2] = {{-1200, -1000}, {-1450, -1000}};
	wall_clip_camera c = camera(0, 0, 0, -1024);
	wall_clip_run run;

	run = wall_clip_strip(piece, 1, &c, 40);
	CHECK_EQ(0, run.count); // 90 degrees reaches 1000 (+100 margin) to the side at 1000 ahead
	c.view_width = 20;		// ~102 degrees: tan(51) = 1.23
	run = wall_clip_strip(piece, 1, &c, 40);
	CHECK_EQ(1, run.count); // reaches 1250 (+100)
}

// The screenshot case: hugging the north wall (96 away), running east (-x)
static void running_along_a_wall_starts_it_in_front(void)
{
	wall_clip_camera c = camera(0, -7904, -1024, 0);
	wall_clip_run low, high;

	north_wall_columns(1000); // x = 1000 (behind) .. -11000 (ahead)
	low = wall_clip_strip(columns, COLUMNS - 1, &c, LOW_MIN);
	high = wall_clip_strip(columns, COLUMNS - 1, &c, HIGH_MIN);

	CHECK_EQ(4, low.first);	 // the piece x = 0 .. -250 straddles the camera
	CHECK_EQ(-40, low.start.x); // slid to 40 in front instead of starting beside the camera
	CHECK_EQ(-8000, low.start.y); // still on the wall
	CHECK_EQ(COLUMNS - 1 - 4, low.count); // everything ahead is kept
	CHECK_EQ(-240, high.start.x);  // the tall strip starts further out
}

// Facing the wall from 96 away: the tall strip would overflow VDP1, and is off screen anyway
static void facing_a_wall_up_close_keeps_only_the_low_strip(void)
{
	wall_clip_camera c = camera(0, -7904, 0, -1024); // north
	wall_clip_run low, high;

	north_wall_columns(6000);
	low = wall_clip_strip(columns, COLUMNS - 1, &c, LOW_MIN);
	high = wall_clip_strip(columns, COLUMNS - 1, &c, HIGH_MIN);

	CHECK_EQ(0, high.count);
	CHECK(low.count > 0);
	CHECK(low.count <= 4); // only the pieces inside the 90 degree view (x within about +-200)
}

static void wall_behind_draws_nothing(void)
{
	wall_clip_camera c = camera(0, -7904, 1024, 0); // west (+x): the wall columns are all at -x
	north_wall_columns(-1000);
	CHECK_EQ(0, wall_clip_strip(columns, COLUMNS - 1, &c, LOW_MIN).count);
}

// Mid turn: looking north-east, the wall ahead-left is in view, nothing starts behind
static void diagonal_view_never_starts_behind(void)
{
	wall_clip_camera c = camera(0, -7000, -724, -724); // north-east (45 degrees), 1000 from the wall
	wall_clip_run low;
	int forward;

	north_wall_columns(4000);
	low = wall_clip_strip(columns, COLUMNS - 1, &c, LOW_MIN);
	CHECK(low.count > 0);
	forward = ((low.start.x - c.eye.x) * c.forward_x + (low.start.y - c.eye.y) * c.forward_y) / 1024;
	CHECK(forward >= LOW_MIN - 1);
	forward = ((low.end.x - c.eye.x) * c.forward_x + (low.end.y - c.eye.y) * c.forward_y) / 1024;
	CHECK(forward >= LOW_MIN - 1);
}

// A cycle 25 across (radius 35 to its corners), seen from the origin facing north (-y)
static void objects_behind_or_beside_are_not_drawn(void)
{
	wall_clip_camera c = camera(0, 0, 0, -1024);
	world_point p;

	p.x = 0;
	p.y = -1000; // straight ahead
	CHECK_EQ(1, wall_clip_object_visible(&c, p, 35, 10));
	p.y = 1000; // straight behind: the bug, it showed on screen
	CHECK_EQ(0, wall_clip_object_visible(&c, p, 35, 10));
	p.y = -40; // so close part of it is behind the near limit
	CHECK_EQ(0, wall_clip_object_visible(&c, p, 35, 10));
	p.y = -50; // all of it just in front
	CHECK_EQ(1, wall_clip_object_visible(&c, p, 35, 10));
	// 1000 ahead, the 90 degree view reaches 1000 (+100 margin) to each side
	p.y = -1000;
	p.x = 1100; // centre just outside, but it reaches back in
	CHECK_EQ(1, wall_clip_object_visible(&c, p, 35, 10));
	p.x = -1200; // wholly outside on the other side
	CHECK_EQ(0, wall_clip_object_visible(&c, p, 35, 10));
}

// On foot, beside a wall running north (x = -300, columns every 250 from y = 1000 to -2000),
// facing north up it from the origin: the cone keeps its near end no further to the side than
// twice its distance in front, so it starts 150 ahead, not 10. Facing the wall itself, nothing's cut
static void the_cone_cuts_a_wall_beside_you(void)
{
	world_point wall[13];
	wall_clip_camera c = camera(0, 0, 0, -1024);
	wall_clip_run run;

	for (int k = 0; k < 13; ++k)
	{
		wall[k].x = -300;
		wall[k].y = (short)(1000 - 250 * k);
	}
	run = wall_clip_strip(wall, 12, &c, 10);
	CHECK_EQ(-10, run.start.y); // only in front: 10 ahead, 300 to the side
	run = wall_clip_strip_in_cone(wall, 12, &c, 10, 32);
	CHECK(run.count > 0);
	CHECK(run.start.y <= -149 && run.start.y >= -151); // 150 ahead: 300 to the side is twice that
	CHECK_EQ(-300, run.start.x);
	CHECK_EQ(-2000, run.end.y); // the far end's well inside the cone: untouched

	// Facing it (east, -x), 40 from it: its middle's straight ahead, its ends slid in to the cone
	// either side, 80 along it each way
	c = camera(-260, -500, -1024, 0);
	run = wall_clip_strip_in_cone(wall, 12, &c, 10, 32);
	CHECK(run.count > 0);
	CHECK(run.start.y >= -421 && run.start.y <= -419);
	CHECK(run.end.y >= -581 && run.end.y <= -579);

	// A single piece wholly beside you, outside the cone: dropped, not drawn back to front
	c = camera(0, 1000, 0, -1024);
	run = wall_clip_strip_in_cone(&wall[3], 1, &c, 10, 32); // y 250..0, 750..1000 ahead, 300 aside
	CHECK_EQ(1, run.count);
	c = camera(0, 300, 0, -1024); // the same piece from 300..50 ahead: the near end's out
	run = wall_clip_strip_in_cone(&wall[3], 1, &c, 10, 32);
	CHECK(run.count == 1 && run.start.y <= 150);
	// A very wide view (boosting further than the game does) still sees it 100 ahead and 300
	// aside, but that's outside the cone all along: dropped
	c = camera(0, 100, 0, -1024);
	c.view_width = 40;
	CHECK_EQ(1, wall_clip_strip(&wall[3], 1, &c, 10).count);
	run = wall_clip_strip_in_cone(&wall[3], 1, &c, 10, 32);
	CHECK_EQ(0, run.count);
}

void run_wall_clip_tests(void)
{
	RUN(the_cone_cuts_a_wall_beside_you);
	RUN(running_along_a_wall_starts_it_in_front);
	RUN(facing_a_wall_up_close_keeps_only_the_low_strip);
	RUN(wall_behind_draws_nothing);
	RUN(diagonal_view_never_starts_behind);
	RUN(wider_view_keeps_pieces_at_the_side);
	RUN(objects_behind_or_beside_are_not_drawn);
}
