#include "game_state.h"

void game_init(game_state *g)
{
	g->phase = GAME_PLAYING;
	g->frames = 0;
}

int game_update(game_state *g, int player_crashed, int pressed)
{
	if (g->phase == GAME_PLAYING)
	{
		if (player_crashed)
		{
			g->phase = GAME_TERMINATED;
			g->frames = 0;
		}
		return 0;
	}

	// Terminated: count the frames, then restart on a press (once it's been up long enough to
	// read) or by itself
	g->frames++;
	if ((pressed && g->frames >= GAME_IGNORE_PRESS_FRAMES) || g->frames >= GAME_TERMINATED_FRAMES)
	{
		game_init(g);
		return 1;
	}
	return 0;
}

#define TEXT_LENGTH ((int)sizeof(GAME_TERMINAL_TEXT) - 1) // sizeof counts the closing 0

// The frame on which character i is typed, and so (i = TEXT_LENGTH) when typing is done: the
// pause, then each character's time. A walk along 26 characters: no division
static int typed_at(int i)
{
	static const char text[] = GAME_TERMINAL_TEXT;
	int frame = GAME_TYPE_START_FRAMES;

	for (int k = 0; k < i; ++k) // i is never past the text's end
		frame += text[k] == '.' ? GAME_TYPE_DOT_FRAMES : GAME_TYPE_FRAMES;
	return frame;
}

int game_typed_chars(const game_state *g)
{
	int typed = 0;

	// Character i shows once its own time has passed: typed_at(i + 1)
	while (typed < TEXT_LENGTH && g->frames >= typed_at(typed + 1))
		typed++;
	return typed;
}

int game_cursor_on(const game_state *g)
{
	const int done = typed_at(TEXT_LENGTH);

	if (g->frames < done)
		return 1; // solid while it types
	return (((g->frames - done) >> GAME_CURSOR_BLINK_SHIFT) & 1) == 0; // then on, off, on...
}
