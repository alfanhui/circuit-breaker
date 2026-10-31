#include "test.h"
#include "../src/game/attract_route.h"
#include "../src/game/attract_text.h"
#include "../src/game/rider.h"
#include "../src/game/arena.h"
#include "../src/game/turn_gate.h"
#include "../src/game/trail.h"

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
		controls.boost = 1; // as the game does: boosting all the way, the gauge kept full
		me.bike.gauge = CYCLE_GAUGE_MAX;
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

static int sink_frames_left; // from the last word spelt to the jump back to the start

// The message ridden as the game rides it (input.c, attract_frame): the cycle put back at the
// start after each, its speed and boost gauge carried on. For each message, how many frames it
// took, and how far ahead (-) or behind (+) of a steady pan at ATTRACT_TEXT_PAN_X256 the cycle
// got. Returns the frames of the last message
static int ride_messages(int messages, int *frames_each, int *ahead_most, int *behind_most)
{
	int count = build();
	attract_route r;
	rider me;
	rider_controls controls = {TURN_NONE, 0};
	int turn;
	world_point start;
	int frames = 0;
	int last_word_end = 0;

	cycle_set_cruise_speed(CYCLE_BASE_SPEED);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
	attract_route_init(&r, waypoints, count);
	start = attract_route_start(&r);
	rider_init(&me, start.x, start.y, (unsigned char)attract_route_start_heading(&r));
	me.bike.bounded = 0;
	*ahead_most = 0;
	*behind_most = 0;
	for (int m = 0; m < messages; ++m)
	{
		frames = 0;
		while (!attract_route_done(&r) && frames < 30000)
		{
			int pan = start.x - ((frames * ATTRACT_TEXT_PAN_X256) >> 8); // where a steady pan is
			int off = me.bike.position.x - pan - ATTRACT_TEXT_PAN_PHASE;

			if (m > 0) // the first message starts from rest: the intro flight covers it
			{
				if (off < *ahead_most)
					*ahead_most = off;
				if (off > *behind_most)
					*behind_most = off;
			}
			if (attract_route_step(&r, &me.bike, &turn) != 0 && r.waypoints[r.next - 1].word_end)
				last_word_end = frames; // a word's spelt: it starts to sink
			controls.turn = (signed char)turn;
			controls.boost = 1;
			me.bike.gauge = CYCLE_GAUGE_MAX;
			rider_step(&me, controls, 0, 0);
			++frames;
		}
		frames_each[m] = frames;
		sink_frames_left = frames - last_word_end;
		// Back to the start, as start_message does: same speed and gauge, same heading
		attract_route_init(&r, waypoints, count);
		me.bike.position = start;
		me.bike.heading = (unsigned char)attract_route_start_heading(&r);
		turn_gate_init(&me.turns);
	}
	return frames;
}

// The spectator camera pans at one steady speed (view.c), not after the cycle, which goes back
// and forth inside each letter. The speed is the message's: a period (15000) in the frames it
// takes to write. Ridden here as the game rides it, every message after the first takes the same
// frames, so the pan stays in step for ever; and the cycle never gets far from the pan's point
#define PAN_ROOM 1500 // the screen shows ~2400 either side of the middle at the spectator's seat
static void a_steady_pan_keeps_up_with_the_message(void)
{
	int frames_each[4];
	int ahead;
	int behind;
	int frames = ride_messages(4, frames_each, &ahead, &behind);

	printf("    frames a message: %d %d %d %d; pan x256 %d (measured %d); cycle %d ahead to %d behind\n",
		   frames_each[0], frames_each[1], frames_each[2], frames_each[3], ATTRACT_TEXT_PAN_X256,
		   (ATTRACT_TEXT_PERIOD * 256 + frames / 2) / frames, -ahead, behind);
	CHECK_EQ(frames_each[1], frames_each[2]); // the same every time, once under way
	CHECK_EQ(frames_each[2], frames_each[3]);
	CHECK_EQ(ATTRACT_TEXT_FRAMES, frames);
	CHECK_EQ((ATTRACT_TEXT_PERIOD * 256 + frames / 2) / frames, ATTRACT_TEXT_PAN_X256);
	CHECK(-ahead < PAN_ROOM);
	CHECK(behind < PAN_ROOM);
	CHECK(-ahead - behind < 200 && behind + ahead < 200); // the phase centres it: as far either way
}

// The last word (SH-2) has sunk right into the floor before the message jumps back to its start
// (input.c): else it would vanish in the jump, in plain sight
static void the_last_word_sinks_before_the_jump_back(void)
{
	int frames_each[2];
	int ahead;
	int behind;

	ride_messages(2, frames_each, &ahead, &behind);
	printf("    frames from the last word to the jump back: %d (sinking takes %d)\n", sink_frames_left,
		   TRAIL_HEIGHT * ATTRACT_SINK_FRAMES);
	CHECK(sink_frames_left > TRAIL_HEIGHT * ATTRACT_SINK_FRAMES);
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
	a_steady_pan_keeps_up_with_the_message();
	the_last_word_sinks_before_the_jump_back();
	too_little_room_fails();
}
