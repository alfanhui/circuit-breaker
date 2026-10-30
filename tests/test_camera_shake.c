#include "test.h"
#include "../src/game/camera_shake.h"

static void still_until_started(void)
{
	camera_shake s;
	int yaw = 99;
	int pitch = 99;

	camera_shake_init(&s);
	CHECK_EQ(0, camera_shake_active(&s));
	camera_shake_step(&s, &yaw, &pitch);
	CHECK_EQ(0, yaw);
	CHECK_EQ(0, pitch);
}

static void first_frame_is_the_full_jolt(void)
{
	camera_shake s;
	int yaw;
	int pitch;

	camera_shake_init(&s);
	camera_shake_start(&s, 256);
	CHECK_EQ(1, camera_shake_active(&s));
	camera_shake_step(&s, &yaw, &pitch);
	CHECK_EQ(256, yaw);	  // straight away: no wind-up
	CHECK_EQ(128, pitch); // half the size
}

static void yaw_flips_every_frame_pitch_every_other(void)
{
	camera_shake s;
	int yaw[4];
	int pitch[4];

	camera_shake_init(&s);
	camera_shake_start(&s, 1000);
	for (int i = 0; i < 4; ++i)
		camera_shake_step(&s, &yaw[i], &pitch[i]);
	CHECK(yaw[0] > 0 && yaw[1] < 0 && yaw[2] > 0 && yaw[3] < 0);
	CHECK(pitch[0] > 0 && pitch[1] > 0 && pitch[2] < 0 && pitch[3] < 0);
}

static void dies_away_and_stops(void)
{
	camera_shake s;
	int yaw;
	int pitch;
	int last = CAMERA_SHAKE_BOT_KILL + 1;
	int frames = 0;

	camera_shake_init(&s);
	camera_shake_start(&s, CAMERA_SHAKE_BOT_KILL);
	while (camera_shake_active(&s) && frames < 100)
	{
		camera_shake_step(&s, &yaw, &pitch);
		int size = yaw < 0 ? -yaw : yaw;

		CHECK(size < last); // smaller every frame
		last = size;
		++frames;
	}
	// The bot kill's jolt is about a fifth of a second: over before it gets in the way
	CHECK(frames >= 8 && frames <= 16);
	camera_shake_step(&s, &yaw, &pitch);
	CHECK_EQ(0, yaw);
	CHECK_EQ(0, pitch);
}

static void a_second_jolt_keeps_the_bigger(void)
{
	camera_shake s;
	int yaw;
	int pitch;

	camera_shake_init(&s);
	camera_shake_start(&s, 1000);
	camera_shake_step(&s, &yaw, &pitch); // now 812
	camera_shake_start(&s, 200);		 // smaller: doesn't shrink it
	camera_shake_step(&s, &yaw, &pitch);
	CHECK_EQ(812, yaw); // and starts again on the + side
	camera_shake_start(&s, 2000);
	camera_shake_step(&s, &yaw, &pitch);
	CHECK_EQ(2000, yaw);
}

void run_camera_shake_tests(void)
{
	still_until_started();
	first_frame_is_the_full_jolt();
	yaw_flips_every_frame_pitch_every_other();
	dies_away_and_stops();
	a_second_jolt_keeps_the_bigger();
}
