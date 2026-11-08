#include "test.h"
#include "../src/game/intro_flight.h"

static int distance(int a, int b)
{
	return a > b ? a - b : b - a;
}

static void starts_face_down_over_the_first_stroke(void)
{
	intro_shot s;

	intro_flight_at(0, &s);
	CHECK_EQ(0, s.rx); // straight down
	CHECK_EQ(0, s.rz); // north up the screen: the letters read upright
	CHECK_EQ(-700, s.ox); // over the S's bottom bar, 700 east of where the cycle starts
	CHECK_EQ(500, s.oy); // on the bar, 500 south of the line the route starts on
	CHECK_EQ(0, s.follow); // held still at the start
	intro_flight_at(-5, &s); // before the start: still the first key
	CHECK_EQ(0, s.rx);
}

// The last frame is the spectator camera exactly (view.c's SPECTATE_*), so the hand-over to it
// can't jump
static void lands_on_the_spectator_seat(void)
{
	intro_shot s;

	intro_flight_at(INTRO_FLIGHT_FRAMES, &s);
	CHECK_EQ(4079, s.ox); // SPECTATE_BACK_X
	CHECK_EQ(7064, s.oy); // SPECTATE_BACK_Y
	CHECK_EQ(3804, s.height);
	CHECK_EQ(11832, s.rx); // DEGtoANG(65.0)
	CHECK_EQ(5461, s.rz);  // DEGtoANG(30.0)
	CHECK_EQ(0, s.roll);
	CHECK_EQ(5461, s.fov); // 30 degrees
	CHECK_EQ(700, s.lead); // SPECTATE_LEAD_X
	CHECK_EQ(INTRO_FOLLOW_FULL, s.follow);
	CHECK_EQ(9700, s.fade_start);
	CHECK_EQ(20000, s.fade_end);
	intro_flight_at(INTRO_FLIGHT_FRAMES + 100, &s); // after the end: stays there
	CHECK_EQ(3804, s.height);
}

// The curve goes through every key: the frame a key's due, the camera is that key
static void passes_through_each_key(void)
{
	intro_shot s;

	intro_flight_at(2 * INTRO_SEGMENT_FRAMES, &s); // alongside the cycle
	CHECK_EQ(1300, s.oy);
	CHECK_EQ(400, s.height);
	CHECK_EQ(INTRO_DEG(66), s.rx);
	intro_flight_at(3 * INTRO_SEGMENT_FRAMES, &s); // the bank
	CHECK_EQ(INTRO_DEG(20), s.roll);
	CHECK(intro_flight_key_name(3)[0] == 'b'); // "bank"
	CHECK(intro_flight_key_name(99)[0] == 's'); // past the end: the last, "seat"
}

// Too much panning was the picture swinging one way, then back: the turn, and the camera's
// place round the cycle, only ever go one way (a unit's wobble allowed for the curve's rounding)
static void never_swings_back(void)
{
	intro_shot before;
	intro_shot now;

	intro_flight_at(0, &before);
	for (int f = 1; f <= INTRO_FLIGHT_FRAMES; ++f)
	{
		intro_flight_at(f, &now);
		CHECK(now.rz >= before.rz - 1);	  // only turning right
		CHECK(now.ox >= before.ox - 1);	  // only moving west of the cycle
		CHECK(now.oy >= before.oy - 1);	  // and further south of it
		CHECK(now.lead >= before.lead - 1);
		before = now;
	}
}

// Every frame's move, and how much that changes from one frame to the next, for each value
// that moves the picture: no snaps (the user saw a 6 degree a frame change as a glitch) and no
// lurches at the keys, where one segment of the curve hands over to the next
static void flies_smoothly(void)
{
	intro_shot before;
	intro_shot now;
	int last_step_height = 0;
	int last_step_rz = 0;
	int worst_step_height = 0;
	int worst_step_angle = 0;
	int worst_lurch_height = 0;
	int worst_lurch_rz = 0;

	intro_flight_at(0, &before);
	for (int f = 1; f <= INTRO_FLIGHT_FRAMES; ++f)
	{
		int step_height;
		int step_rz;

		intro_flight_at(f, &now);
		step_height = now.height - before.height;
		step_rz = now.rz - before.rz;
		if (distance(step_height, 0) > worst_step_height)
			worst_step_height = distance(step_height, 0);
		if (distance(now.rx, before.rx) > worst_step_angle)
			worst_step_angle = distance(now.rx, before.rx);
		if (distance(step_rz, 0) > worst_step_angle)
			worst_step_angle = distance(step_rz, 0);
		if (distance(now.roll, before.roll) > worst_step_angle)
			worst_step_angle = distance(now.roll, before.roll);
		if (f > 1 && distance(step_height, last_step_height) > worst_lurch_height)
			worst_lurch_height = distance(step_height, last_step_height);
		if (f > 1 && distance(step_rz, last_step_rz) > worst_lurch_rz)
			worst_lurch_rz = distance(step_rz, last_step_rz);
		last_step_height = step_height;
		last_step_rz = step_rz;
		before = now;
	}
	CHECK(worst_step_height <= 40);	 // units a frame
	CHECK(worst_step_angle <= 182);	 // a degree a frame at most
	CHECK(worst_lurch_height <= 2);	 // the climb's speed changes gently, keys included
	CHECK(worst_lurch_rz <= 4);		 // and so does the turn's
	printf("    worst a frame: height %d, angle %d; change of speed: height %d, turn %d\n", worst_step_height,
		   worst_step_angle, worst_lurch_height, worst_lurch_rz);
}

// It starts from rest, picking up speed, and settles into the seat: hardly any move on the
// first and last frames
static void starts_and_ends_at_rest(void)
{
	intro_shot a;
	intro_shot b;

	intro_flight_at(0, &a);
	intro_flight_at(1, &b);
	CHECK(distance(a.height, b.height) <= 1);
	CHECK(distance(a.follow, b.follow) <= 1);
	intro_flight_at(INTRO_FLIGHT_FRAMES - 1, &a);
	intro_flight_at(INTRO_FLIGHT_FRAMES, &b);
	CHECK(distance(a.height, b.height) <= 4); // its last 2800 climb ends in a step of a few units
	CHECK(distance(a.rz, b.rz) <= 4); // 0.02 degrees
	CHECK(distance(a.oy, b.oy) <= 4);
}

// Never lower than well over a trail's top (60), and never looking above the horizon; follow
// stays a share
static void stays_above_the_trails(void)
{
	intro_shot s;
	int lowest = 100000;

	for (int f = 0; f <= INTRO_FLIGHT_FRAMES; ++f)
	{
		intro_flight_at(f, &s);
		if (s.height < lowest)
			lowest = s.height;
		CHECK(s.rx >= 0 && s.rx < INTRO_DEG(90));
		CHECK(s.follow >= 0 && s.follow <= INTRO_FOLLOW_FULL);
	}
	CHECK(lowest >= 120);
	printf("    lowest: %d\n", lowest);
}

void run_intro_flight_tests(void)
{
	RUN(starts_face_down_over_the_first_stroke);
	RUN(lands_on_the_spectator_seat);
	RUN(passes_through_each_key);
	RUN(never_swings_back);
	RUN(flies_smoothly);
	RUN(starts_and_ends_at_rest);
	RUN(stays_above_the_trails);
}
