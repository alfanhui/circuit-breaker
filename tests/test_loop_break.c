#include "test.h"
#include "../src/game/loop_break.h"

static void nothing_before_a_press(void)
{
	loop_break b;
	int inner;
	int outer;

	loop_break_init(&b);
	CHECK_EQ(0, loop_break_broken(&b));
	CHECK_EQ(0, loop_break_burst(&b, &inner, &outer));
	CHECK_EQ(0, loop_break_flash(&b));
}

static void the_third_press_breaks_it(void)
{
	loop_break b;

	loop_break_init(&b);
	CHECK_EQ(1, loop_break_press(&b));
	CHECK_EQ(0, loop_break_broken(&b));
	CHECK_EQ(2, loop_break_press(&b));
	CHECK_EQ(0, loop_break_broken(&b));
	CHECK_EQ(3, loop_break_press(&b));
	CHECK_EQ(1, loop_break_broken(&b));
	CHECK_EQ(0, loop_break_press(&b)); // after that, presses don't count
	CHECK_EQ(1, loop_break_broken(&b));
}

// The burst shoots out, slowing, while its inner ends chase after it: the rays shorten to nothing
static void the_burst_flies_out_and_thins_away(void)
{
	loop_break b;
	int inner;
	int outer;
	int last_outer = -1;
	int frames = 0;

	loop_break_init(&b);
	loop_break_press(&b);
	while (loop_break_burst(&b, &inner, &outer) && frames < 100)
	{
		CHECK(outer >= last_outer); // never back in
		CHECK(inner <= outer);
		CHECK(outer <= LOOP_BREAK_BURST_REACH);
		last_outer = outer;
		loop_break_update(&b);
		frames++;
	}
	CHECK_EQ(LOOP_BREAK_BURST_FRAMES, frames);
	CHECK(last_outer > LOOP_BREAK_BURST_REACH * 9 / 10); // nearly all the way out by the end
}

// Fast at first: over half its reach in the first quarter of its life
static void the_burst_is_quick_off_the_mark(void)
{
	loop_break b;
	int inner;
	int outer;

	loop_break_init(&b);
	loop_break_press(&b);
	for (int i = 0; i < LOOP_BREAK_BURST_FRAMES / 4; ++i)
		loop_break_update(&b);
	loop_break_burst(&b, &inner, &outer);
	CHECK(outer > LOOP_BREAK_BURST_REACH / 2);
}

// Each press hits harder: a bigger burst and a brighter flash; the third flashes white
static void each_press_is_harder(void)
{
	loop_break b;
	int inner;
	int outer[4];
	int flash[4];

	loop_break_init(&b);
	for (int p = 1; p <= 3; ++p)
	{
		loop_break_press(&b);
		flash[p] = loop_break_flash(&b);
		for (int i = 0; i < LOOP_BREAK_BURST_FRAMES - 1; ++i)
			loop_break_update(&b);
		loop_break_burst(&b, &inner, &outer[p]);
	}
	CHECK(outer[2] > outer[1]);
	CHECK(outer[3] > outer[2]);
	CHECK(flash[2] > flash[1]);
	CHECK_EQ(255, flash[3]);
}

static void the_flash_fades_quickly(void)
{
	loop_break b;
	int last = 256;

	loop_break_init(&b);
	loop_break_press(&b);
	for (int i = 0; i < LOOP_BREAK_FLASH_FRAMES; ++i)
	{
		int now = loop_break_flash(&b);

		CHECK(now > 0 && now < last);
		last = now;
		loop_break_update(&b);
	}
	CHECK_EQ(0, loop_break_flash(&b));
}

void run_loop_break_tests(void)
{
	nothing_before_a_press();
	the_third_press_breaks_it();
	the_burst_flies_out_and_thins_away();
	the_burst_is_quick_off_the_mark();
	each_press_is_harder();
	the_flash_fades_quickly();
}
