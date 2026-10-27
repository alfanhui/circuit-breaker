#include "test.h"
#include "../src/game/press_latch.h"

// Same bits as DIGI_LEFT / DIGI_RIGHT, but this file can't include the Saturn headers
#define LEFT 0x4000
#define RIGHT 0x8000

static void press_is_reported_once(void)
{
	press_latch l;
	press_latch_init(&l);

	press_latch_sample(&l, LEFT);
	CHECK_EQ(LEFT, press_latch_take(&l));

	press_latch_sample(&l, LEFT); // still held
	CHECK_EQ(0, press_latch_take(&l));
}

static void nothing_pressed_reports_nothing(void)
{
	press_latch l;
	press_latch_init(&l);
	press_latch_sample(&l, 0);
	CHECK_EQ(0, press_latch_take(&l));
	CHECK_EQ(0, press_latch_take(&l)); // a frame without a vblank in between
}

// The bug this module fixes: a slow game frame spans two vblanks. Comparing only the
// last two samples (pressed, still pressed) says "no change" and the turn is lost.
static void press_survives_a_slow_frame(void)
{
	press_latch l;
	press_latch_init(&l);

	press_latch_sample(&l, LEFT); // vblank 1: pressed
	press_latch_sample(&l, LEFT); // vblank 2: still held, no game frame in between
	CHECK_EQ(LEFT, press_latch_take(&l));
}

// A quick tap: pressed and released before the game frame looks.
static void tap_between_frames_is_reported(void)
{
	press_latch l;
	press_latch_init(&l);

	press_latch_sample(&l, RIGHT);
	press_latch_sample(&l, 0);
	CHECK_EQ(RIGHT, press_latch_take(&l));
}

static void release_and_press_again_is_a_new_press(void)
{
	press_latch l;
	press_latch_init(&l);

	press_latch_sample(&l, LEFT);
	press_latch_take(&l);
	press_latch_sample(&l, 0);
	CHECK_EQ(0, press_latch_take(&l)); // releasing isn't a press
	press_latch_sample(&l, LEFT);
	CHECK_EQ(LEFT, press_latch_take(&l));
}

static void buttons_are_independent(void)
{
	press_latch l;
	press_latch_init(&l);

	press_latch_sample(&l, LEFT);
	press_latch_sample(&l, LEFT | RIGHT); // right joins while left is held
	CHECK_EQ(LEFT | RIGHT, press_latch_take(&l));

	press_latch_sample(&l, RIGHT); // left released, right held
	CHECK_EQ(0, press_latch_take(&l));
}

// A press that lands after take() has read toggles must wait for the next frame,
// not disappear. Simulated by sampling between two takes.
static void press_during_take_is_kept_for_next_frame(void)
{
	press_latch l;
	press_latch_init(&l);

	CHECK_EQ(0, press_latch_take(&l));
	press_latch_sample(&l, RIGHT);
	CHECK_EQ(RIGHT, press_latch_take(&l));
	CHECK_EQ(0, press_latch_take(&l));
}

// Documents the known limit: two presses of the same button before one take cancel out.
static void double_tap_inside_one_frame_is_the_known_limit(void)
{
	press_latch l;
	press_latch_init(&l);

	press_latch_sample(&l, LEFT);
	press_latch_sample(&l, 0);
	press_latch_sample(&l, LEFT);
	CHECK_EQ(0, press_latch_take(&l));
}

void run_press_latch_tests(void)
{
	RUN(press_is_reported_once);
	RUN(nothing_pressed_reports_nothing);
	RUN(press_survives_a_slow_frame);
	RUN(tap_between_frames_is_reported);
	RUN(release_and_press_again_is_a_new_press);
	RUN(buttons_are_independent);
	RUN(press_during_take_is_kept_for_next_frame);
	RUN(double_tap_inside_one_frame_is_the_known_limit);
}
