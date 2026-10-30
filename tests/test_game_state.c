#include "test.h"
#include "../src/game/game_state.h"

static void a_crash_terminates_the_player(void)
{
	game_state g;

	game_init(&g);
	CHECK_EQ(0, game_update(&g, 0, 0));
	CHECK_EQ(GAME_PLAYING, g.phase);
	CHECK_EQ(0, game_update(&g, 1, 0));
	CHECK_EQ(GAME_TERMINATED, g.phase);
}

static void restarts_by_itself_after_3_seconds(void)
{
	game_state g;
	int frames = 0;

	game_init(&g);
	game_update(&g, 1, 0);
	while (!game_update(&g, 0, 0) && frames < 1000)
		frames++;
	CHECK_EQ(GAME_TERMINATED_FRAMES - 1, frames); // the restarting frame isn't counted in the loop
	CHECK_EQ(GAME_PLAYING, g.phase);
}

static void a_press_restarts_it_once_it_can_be_read(void)
{
	game_state g;

	game_init(&g);
	game_update(&g, 1, 0);
	// A press straight away (a turn that came a moment too late) doesn't skip the screen
	CHECK_EQ(0, game_update(&g, 0, 1));
	for (int i = 0; i < GAME_IGNORE_PRESS_FRAMES; ++i)
		game_update(&g, 0, 0);
	CHECK_EQ(1, game_update(&g, 0, 1));
	CHECK_EQ(GAME_PLAYING, g.phase);
}

// The text is typed out: nothing at first (the cursor alone), then a character at a time, the
// dots slower, all of it within the 3 s
static void the_text_is_typed_out(void)
{
	game_state g;
	int last = 0;
	int frames_for_dot = 0;
	int typed;

	game_init(&g);
	game_update(&g, 1, 0);
	CHECK_EQ(0, game_typed_chars(&g));
	for (int i = 0; i < GAME_TERMINATED_FRAMES - 1; ++i)
	{
		game_update(&g, 0, 0);
		typed = game_typed_chars(&g);
		CHECK(typed == last || typed == last + 1); // one character at a time
		if (typed == last && typed >= 23 && typed < 26)
			frames_for_dot++; // waiting on a dot
		last = typed;
	}
	CHECK_EQ(26, last); // all of "SH-2 process terminated..." in the time
	CHECK(frames_for_dot > 3 * GAME_TYPE_FRAMES); // the dots took longer than letters
}

// Solid while typing, then on and off every 32 frames
static void the_cursor_flashes_once_typed(void)
{
	game_state g;
	int changes = 0;
	int last = 1;

	game_init(&g);
	game_update(&g, 1, 0);
	for (int i = 0; i < GAME_TERMINATED_FRAMES - 1; ++i)
	{
		game_update(&g, 0, 0);
		if (game_typed_chars(&g) < 26)
			CHECK_EQ(1, game_cursor_on(&g));
		if (game_cursor_on(&g) != last)
			changes++;
		last = game_cursor_on(&g);
	}
	// Typing ends at frame 120: off at 152, on at 184 (after the restart). One change in the time
	CHECK_EQ(1, changes);
}

void run_game_state_tests(void)
{
	RUN(a_crash_terminates_the_player);
	RUN(restarts_by_itself_after_3_seconds);
	RUN(a_press_restarts_it_once_it_can_be_read);
	RUN(the_text_is_typed_out);
	RUN(the_cursor_flashes_once_typed);
}
