#include "test.h"
#include "../src/game/attract_route.h"
#include "../src/game/attract_text.h"
#include "../src/game/rider.h"
#include "../src/game/arena.h"
#include "../src/game/turn_gate.h"

static attract_waypoint waypoints[ATTRACT_TEXT_MAX_WAYPOINTS];

static world_point pt(short x, short y)
{
	world_point p;

	p.x = x;
	p.y = y;
	return p;
}

static int build(void)
{
	return attract_text_build(ATTRACT_TEXT, pt(0, 0), waypoints, ATTRACT_TEXT_MAX_WAYPOINTS);
}

static void headings_between_points(void)
{
	CHECK_EQ(HEADING_NORTH, attract_route_heading(pt(0, 0), pt(0, -10)));
	CHECK_EQ(HEADING_SOUTH, attract_route_heading(pt(0, 0), pt(0, 10)));
	CHECK_EQ(HEADING_EAST, attract_route_heading(pt(0, 0), pt(-10, 0))); // east is -x
	CHECK_EQ(HEADING_WEST, attract_route_heading(pt(0, 0), pt(10, 0)));
}

// Every leg is straight along an axis, long enough for the turn gate, and never turns straight
// back on the one before (the cycle can't)
static void the_message_can_be_ridden(void)
{
	int count = build();
	int previous = -1;

	CHECK(count > 2);
	CHECK(count <= ATTRACT_TEXT_MAX_WAYPOINTS);
	for (int i = 1; i < count; ++i)
	{
		world_point a = waypoints[i - 1].p;
		world_point b = waypoints[i].p;
		int length = (b.x - a.x) + (b.y - a.y);
		int heading;

		if (length < 0)
			length = -length;
		CHECK(a.x == b.x || a.y == b.y); // straight
		CHECK(length >= TURN_GATE_MIN_GAP); // long enough to turn again at its end
		heading = attract_route_heading(a, b);
		if (previous >= 0)
			CHECK(heading != ((previous + 2) & 3)); // not straight back
		previous = heading;
	}
}

// The strokes the font draws: S 1, E 2, G 1, A 2, S 1, A 2, T 2, U 1, R 2, N 1, S 1, H 3, - 1, 2 1;
// and three words, each ending on a pen-down point
static void it_writes_every_stroke_and_word(void)
{
	int count = build();
	int strokes = 0;
	int words = 0;

	for (int i = 1; i < count; ++i)
	{
		if (waypoints[i].pen && !waypoints[i - 1].pen)
			strokes++;
		if (waypoints[i].word_end)
		{
			words++;
			CHECK_EQ(1, waypoints[i].pen);
		}
	}
	CHECK_EQ(21, strokes);
	CHECK_EQ(3, words);
	CHECK_EQ(0, waypoints[0].pen);
	CHECK_EQ(0, waypoints[count - 1].pen); // it rides to the end with no trail
}

// It ends where it began, a period east (-x), riding the same way, so it can be put back
static void it_ends_a_period_on_riding_the_same_way(void)
{
	int count = build();

	CHECK_EQ(waypoints[0].p.x - ATTRACT_TEXT_PERIOD, waypoints[count - 1].p.x);
	CHECK_EQ(waypoints[0].p.y, waypoints[count - 1].p.y);
	CHECK_EQ(attract_route_heading(waypoints[0].p, waypoints[1].p),
			 attract_route_heading(waypoints[count - 2].p, waypoints[count - 1].p));
	// The whole message is written before the end: room after the last word for it to sink
	for (int i = 0; i < count; ++i)
		if (waypoints[i].pen)
			CHECK(waypoints[i].p.x > waypoints[count - 1].p.x + 900);
}

// Rides the whole message as a ghost, driven only by the route's presses: it reaches every
// waypoint in turn, put on each exactly, and ends on the last
static void a_cycle_rides_it_all(void)
{
	int count = build();
	attract_route r;
	rider me;
	rider_controls controls = {TURN_NONE, 0};
	int reached = 0;
	int frames = 0;
	int turn;
	world_point start;

	cycle_set_cruise_speed(CYCLE_BASE_SPEED);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
	attract_route_init(&r, waypoints, count);
	start = attract_route_start(&r);
	CHECK_EQ(0, attract_route_pen(&r));
	rider_init(&me, start.x, start.y, (unsigned char)attract_route_start_heading(&r));
	me.bike.bounded = 0;
	while (!attract_route_done(&r) && frames < 30000)
	{
		const attract_waypoint *here = attract_route_step(&r, &me.bike, &turn);

		if (here)
		{
			CHECK(here == &waypoints[reached + 1]); // in order, none skipped
			CHECK_EQ(here->p.x, me.bike.position.x);
			CHECK_EQ(here->p.y, me.bike.position.y);
			reached++;
		}
		controls.turn = (signed char)turn;
		controls.boost = (unsigned char)!attract_route_pen(&r); // as the game does: quicker with the pen up
		if (turn != TURN_NONE)
			CHECK_EQ(turn, rider_step(&me, controls, 0, 0)); // the turn gate never holds a turn back
		else
			rider_step(&me, controls, 0, 0);
		++frames;
	}
	CHECK_EQ(count - 1, reached); // the last one too (put on it exactly, checked above)
	turn = 99;
	CHECK(attract_route_step(&r, &me.bike, &turn) == 0); // nothing more
	CHECK_EQ(TURN_NONE, turn);
}

static void too_little_room_fails(void)
{
	CHECK_EQ(0, attract_text_build(ATTRACT_TEXT, pt(0, 0), waypoints, 10));
}

void run_attract_route_tests(void)
{
	headings_between_points();
	the_message_can_be_ridden();
	it_writes_every_stroke_and_word();
	it_ends_a_period_on_riding_the_same_way();
	a_cycle_rides_it_all();
	too_little_room_fails();
}
