#include "test.h"
#include "../src/game/turn_ease.h"
#include "../src/game/cycle.h"

// The boost's FOV kick, as view.c does it: the FOV (in ANGLE, 182 a degree) eases with
// ease_out_step towards 90 degrees + 12 x boost level / 16, while the boost level ramps
#define FOV_90 0x4000
#define FOV_BOOST 2184 // 12 degrees
#define FOV_SHARE 2
#define FOV_MIN_STEP 32
#define DEGREE 182

// Frames of holding boost then letting go; returns the biggest change in one frame, and the FOV
// at the end in *fov_out
static int boost_fov_run(int held, int released, int *fov_out)
{
	int level = 0;
	int fov = FOV_90;
	int biggest = 0;

	for (int frame = 0; frame < held + released; ++frame)
	{
		int next;

		if (frame < held) // the boost level's ramps (cycle.c)
			level = level + CYCLE_BOOST_RAMP_UP > CYCLE_BOOST_FULL ? CYCLE_BOOST_FULL : level + CYCLE_BOOST_RAMP_UP;
		else
			level = level > CYCLE_BOOST_RAMP_DOWN ? level - CYCLE_BOOST_RAMP_DOWN : 0;
		next = ease_out_step(fov, FOV_90 + ((level * FOV_BOOST) >> 4), FOV_SHARE, FOV_MIN_STEP);
		if (next - fov > biggest)
			biggest = next - fov;
		if (fov - next > biggest)
			biggest = fov - next;
		fov = next;
	}
	*fov_out = fov;
	return biggest;
}

static void boost_fov_widens_smoothly(void)
{
	int fov;

	// It used to follow the boost level straight: 6 degrees a frame, which looked like a glitch
	CHECK(boost_fov_run(40, 60, &fov) < DEGREE); // never a whole degree in one frame
	CHECK(boost_fov_run(1, 0, &fov) < DEGREE / 4); // and the first frame barely moves
}

static void boost_fov_settles_and_returns(void)
{
	int fov;

	boost_fov_run(40, 0, &fov);
	CHECK_EQ(FOV_90 + FOV_BOOST, fov); // fully widened after 2/3 s of boost
	boost_fov_run(40, 60, &fov);
	CHECK_EQ(FOV_90, fov); // and back to exactly 90 degrees a second after letting go
}

#define QUARTER 0x4000 // 90 degrees

// Steps from 0 to target, counting frames until the camera lands
static int frames_to_land(int target)
{
	int angle = 0;
	int frames = 0;

	while (angle != target && frames < 100)
	{
		angle = turn_ease_step(angle, target);
		++frames;
	}
	return frames;
}

static void at_the_heading_stays_put(void)
{
	CHECK_EQ(QUARTER, turn_ease_step(QUARTER, QUARTER));
}

static void first_frame_covers_most_of_the_turn(void)
{
	// The old linear turn did 1/10 in the first frame; ease-out does 6/16
	CHECK_EQ(QUARTER * 6 / 16, turn_ease_step(0, QUARTER));
	CHECK(turn_ease_step(0, QUARTER) > QUARTER / 3);
}

static void quarter_turn_lands_in_ten_frames(void)
{
	CHECK_EQ(10, frames_to_land(QUARTER)); // about as long as the old turn (11): mostly the shape changed
	CHECK_EQ(10, frames_to_land(-QUARTER));
}

static void left_mirrors_right(void)
{
	int right = 0;
	int left = 0;

	for (int i = 0; i < 12; ++i)
	{
		right = turn_ease_step(right, QUARTER);
		left = turn_ease_step(left, -QUARTER);
		CHECK_EQ(right, -left);
	}
}

static void steps_shrink_and_never_overshoot(void)
{
	int angle = 0;
	int last_step = QUARTER;

	while (angle != QUARTER)
	{
		int next = turn_ease_step(angle, QUARTER);
		CHECK(next > angle);	// always moving towards the heading...
		CHECK(next <= QUARTER); // ...never past it
		CHECK(next - angle <= last_step); // ease-out: no step bigger than the one before
		last_step = next - angle;
		angle = next;
	}
}

static void tail_never_crawls(void)
{
	// Just outside the snap distance, the step is still the minimum, not a tiny share of it
	CHECK_EQ(TURN_EASE_MIN_STEP, turn_ease_step(0, TURN_EASE_MIN_STEP + 1));
	CHECK_EQ(TURN_EASE_MIN_STEP + 1, turn_ease_step(1, TURN_EASE_MIN_STEP + 1)); // within one step: lands
}

static void second_turn_mid_ease_carries_on(void)
{
	// Turn right, then right again after 2 frames: the camera heads for 180 without stopping at 90
	int angle = turn_ease_step(turn_ease_step(0, QUARTER), QUARTER);
	int next = turn_ease_step(angle, 2 * QUARTER);

	CHECK(next - angle > QUARTER / 3); // the new turn gets its own big first step
	CHECK(frames_to_land(2 * QUARTER) <= 12);
}

static void linear_turn_is_the_same_step_every_frame(void)
{
	int angle = 0;
	int frames = 0;

	CHECK_EQ(TURN_LINEAR_STEP, turn_linear_step(0, QUARTER));
	CHECK_EQ(-TURN_LINEAR_STEP, turn_linear_step(0, -QUARTER));
	while (angle != QUARTER && frames < 100)
	{
		angle = turn_linear_step(angle, QUARTER);
		++frames;
	}
	CHECK_EQ(11, frames); // 10 x 1638 falls 4 short of 0x4000, so it lands a frame later
}

void run_turn_ease_tests(void)
{
	RUN(linear_turn_is_the_same_step_every_frame);
	RUN(at_the_heading_stays_put);
	RUN(first_frame_covers_most_of_the_turn);
	RUN(quarter_turn_lands_in_ten_frames);
	RUN(left_mirrors_right);
	RUN(steps_shrink_and_never_overshoot);
	RUN(tail_never_crawls);
	RUN(second_turn_mid_ease_carries_on);
	RUN(boost_fov_widens_smoothly);
	RUN(boost_fov_settles_and_returns);
}
