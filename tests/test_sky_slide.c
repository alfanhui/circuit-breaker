// The sky slid with you (game/sky_slide.h): handed to the sky in the corridor's frame, it moves a
// point by just the slide, whichever wall the corridor's behind; and it eases back to nothing
#include "test.h"
#include "../src/game/sky_slide.h"
#include "../src/game/corridor.h"

// For every side: a landmark's foot placed with the slid frame is the slide away from where it stands
static void the_slide_moves_the_sky_by_itself_in_the_world(void)
{
	sky_slide s;

	sky_slide_init(&s);
	sky_slide_add(&s, 3000, -1750);
	for (int side = 0; side < 4; ++side)
	{
		int out, across;
		world_point at, slid;

		sky_slide_frame(&s, side, &out, &across);
		at = corridor_point_behind(side, 0, 16500, -16500);
		slid = corridor_point_behind(side, out, 16500, -16500 + across); // (the wall's distance takes `out`)
		CHECK_EQ(at.x + 3000, slid.x);
		CHECK_EQ(at.y - 1750, slid.y);
	}
}

// Two slides add up (the attract loop's to the middle, then a round's)
static void slides_add_up(void)
{
	sky_slide s;

	sky_slide_init(&s);
	sky_slide_add(&s, 1000, 500);
	sky_slide_add(&s, -250, 2000);
	CHECK_EQ(750, s.x);
	CHECK_EQ(2500, s.y);
}

// It eases back SKY_SLIDE_EASE a frame each way, lands on 0 exactly, and stays there
static void it_eases_back_to_nothing(void)
{
	sky_slide s;
	int frames = 0;

	sky_slide_init(&s);
	sky_slide_add(&s, 8000, -7);
	sky_slide_step(&s);
	CHECK_EQ(8000 - SKY_SLIDE_EASE, s.x);
	CHECK_EQ(-1, s.y);
	sky_slide_step(&s);
	CHECK_EQ(0, s.y); // (never past it)
	while (s.x != 0 && frames < 100000)
	{
		sky_slide_step(&s);
		++frames;
	}
	CHECK_EQ(0, s.x);
	CHECK(frames < 60 * 30); // half the arena back in under 30 s
	sky_slide_step(&s);
	CHECK_EQ(0, s.x);
}

void run_sky_slide_tests(void)
{
	RUN(the_slide_moves_the_sky_by_itself_in_the_world);
	RUN(slides_add_up);
	RUN(it_eases_back_to_nothing);
}
