#include "test.h"
#include "../src/game/status_message.h"

static void nothing_until_shown(void)
{
	status_message s;

	status_init(&s);
	CHECK_EQ(0, status_visible(&s));
	CHECK_EQ(0, status_cursor_on(&s));
	status_update(&s); // nothing to move on
	CHECK_EQ(0, status_visible(&s));
}

static void types_a_character_every_two_frames(void)
{
	status_message s;

	status_init(&s);
	status_show(&s, "abcd");
	CHECK_EQ(1, status_visible(&s));
	CHECK_EQ(0, status_typed_chars(&s));
	CHECK_EQ(1, status_cursor_on(&s));
	status_update(&s);
	CHECK_EQ(0, status_typed_chars(&s));
	status_update(&s);
	CHECK_EQ(1, status_typed_chars(&s));
	for (int i = 0; i < 6; ++i)
		status_update(&s);
	CHECK_EQ(4, status_typed_chars(&s)); // all of it at frame 8
	for (int i = 0; i < 10; ++i)
		status_update(&s);
	CHECK_EQ(4, status_typed_chars(&s)); // and never more
}

static void cursor_solid_while_typing_then_flashes(void)
{
	status_message s;

	status_init(&s);
	status_show(&s, "abcd"); // typed by frame 8
	for (int i = 0; i < 8; ++i)
	{
		CHECK_EQ(1, status_cursor_on(&s));
		status_update(&s);
	}
	CHECK_EQ(1, status_cursor_on(&s)); // frames 8..23: on
	for (int i = 0; i < 16; ++i)
		status_update(&s);
	CHECK_EQ(0, status_cursor_on(&s)); // 24..39: off
	for (int i = 0; i < 16; ++i)
		status_update(&s);
	CHECK_EQ(1, status_cursor_on(&s)); // then on again
}

static void goes_after_the_hold(void)
{
	status_message s;
	int frames = 0;

	status_init(&s);
	status_show(&s, STATUS_ENEMY_ELIMINATED); // 24 characters: typed in 48 frames
	while (status_visible(&s) && frames < 1000)
	{
		status_update(&s);
		++frames;
	}
	CHECK_EQ(48 + STATUS_HOLD_FRAMES, frames);
}

static void a_new_message_starts_again(void)
{
	status_message s;

	status_init(&s);
	status_show(&s, "abcd");
	for (int i = 0; i < 20; ++i)
		status_update(&s);
	status_show(&s, "xy");
	CHECK_EQ(0, status_typed_chars(&s));
	CHECK_EQ(1, status_cursor_on(&s));
	CHECK(s.text[0] == 'x');
}

void run_status_message_tests(void)
{
	nothing_until_shown();
	types_a_character_every_two_frames();
	cursor_solid_while_typing_then_flashes();
	goes_after_the_hold();
	a_new_message_starts_again();
}
