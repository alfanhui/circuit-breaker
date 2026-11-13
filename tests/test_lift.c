#include "test.h"
#include "../src/game/lift.h"

#define LOW (-1400)
#define HIGH 3850

static void run(lift *l, int frames, bool on_pad)
{
	for (int i = 0; i < frames; ++i)
		lift_update(l, on_pad);
}

static void walking_over_the_pad_does_not_take_you(void)
{
	lift l;

	lift_init(&l, LOW, HIGH);
	run(&l, LIFT_ARM_FRAMES - 1, true);
	CHECK(!lift_carries(&l));
	run(&l, 1, false); // stepped off in time
	CHECK_EQ(LIFT_WAITING, l.state);
	// And the wait starts again from nothing next time
	run(&l, LIFT_ARM_FRAMES - 1, true);
	CHECK(!lift_carries(&l));
	CHECK_EQ(LOW, lift_height(&l));
}

static void standing_on_it_takes_you_up_and_back(void)
{
	lift l;

	lift_init(&l, LOW, HIGH);
	run(&l, LIFT_ARM_FRAMES, true);
	CHECK_EQ(LIFT_UP, l.state);
	CHECK(lift_carries(&l));
	CHECK_EQ(LOW, lift_height(&l));
	run(&l, LIFT_RIDE_FRAMES / 2, true);
	CHECK_EQ((LOW + HIGH) / 2, lift_height(&l)); // halfway in half the time
	run(&l, LIFT_RIDE_FRAMES / 2, true);
	CHECK_EQ(LIFT_HELD, l.state);
	CHECK_EQ(HIGH, lift_height(&l));
	run(&l, LIFT_HOLD_FRAMES, true);
	CHECK_EQ(LIFT_DOWN, l.state);
	run(&l, LIFT_RIDE_FRAMES, true);
	CHECK_EQ(LIFT_ARRIVED, l.state);
	CHECK_EQ(LOW, lift_height(&l));
	CHECK(!lift_carries(&l));
}

// It eases away and eases in: never a jump, slow at both ends, never backwards
static void the_ride_is_smooth(void)
{
	lift l;
	int was, fastest = 0;

	lift_init(&l, LOW, HIGH);
	run(&l, LIFT_ARM_FRAMES, true);
	was = lift_height(&l);
	for (int i = 0; i < LIFT_RIDE_FRAMES; ++i)
	{
		int now, step;

		lift_update(&l, true);
		now = lift_height(&l);
		step = now - was;
		CHECK(step >= 0);
		CHECK(step <= 17); // 1.5 x the average 10.25, and the rounding
		if (i < 8 || i >= LIFT_RIDE_FRAMES - 8)
			CHECK(step <= 2);
		if (step > fastest)
			fastest = step;
		was = now;
	}
	CHECK(fastest >= 14);
	CHECK_EQ(HIGH, was);
}

// Back down, it stays put while you're still on the pad, and goes again once you've been off it
static void it_waits_for_you_to_step_off(void)
{
	lift l;

	lift_init(&l, LOW, HIGH);
	run(&l, LIFT_ARM_FRAMES + LIFT_RIDE_FRAMES + LIFT_HOLD_FRAMES + LIFT_RIDE_FRAMES, true);
	CHECK_EQ(LIFT_ARRIVED, l.state);
	run(&l, 600, true);
	CHECK_EQ(LIFT_ARRIVED, l.state);
	run(&l, 1, false);
	CHECK_EQ(LIFT_WAITING, l.state);
	run(&l, LIFT_ARM_FRAMES, true);
	CHECK_EQ(LIFT_UP, l.state);
}

void run_lift_tests(void)
{
	RUN(walking_over_the_pad_does_not_take_you);
	RUN(standing_on_it_takes_you_up_and_back);
	RUN(the_ride_is_smooth);
	RUN(it_waits_for_you_to_step_off);
}
