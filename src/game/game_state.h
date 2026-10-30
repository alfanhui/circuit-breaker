#ifndef __GAME_STATE_H__
#define __GAME_STATE_H__

/*
 * The round: playing, or terminated. Pure C, unit-tested on the PC.
 *
 * A crash terminates the player's process (the trace it was laying). The game goes to a black
 * screen where "SH-2 process terminated..." is typed out in green terminal text behind a block
 * cursor, then restarts on the next button press, or by itself after GAME_TERMINATED_FRAMES.
 *
 * The typing: after a short pause, a character every GAME_TYPE_FRAMES, but each '.' takes
 * GAME_TYPE_DOT_FRAMES, like a terminal still working on it. The cursor is solid while it types
 * and flashes once it's done. 2 s of typing (15 + 23 x 3 + 3 x 12 frames), then it flashes for the
 * last second.
 *
 * Presses in the first GAME_IGNORE_PRESS_FRAMES don't count: a turn pressed a moment too late,
 * already on its way when the crash happened, mustn't skip the screen before it's been read.
 */

#define GAME_TERMINATED_FRAMES 180	// 3 s, then it restarts by itself
#define GAME_IGNORE_PRESS_FRAMES 20 // a third of a second before a press can restart it
#define GAME_CURSOR_BLINK_SHIFT 5	// the cursor shows for 32 frames, hides for 32: >> 5 & 1

#define GAME_TERMINAL_TEXT "SH-2 process terminated..."
#define GAME_TYPE_START_FRAMES 15 // the cursor alone first, as if the terminal had just woken
#define GAME_TYPE_FRAMES 3		  // per character
#define GAME_TYPE_DOT_FRAMES 12	  // per '.'

#define GAME_PLAYING 0
#define GAME_TERMINATED 1

typedef struct
{
	unsigned char phase;   // GAME_PLAYING or GAME_TERMINATED
	unsigned short frames; // frames since it was terminated
} game_state;

void game_init(game_state *g);

// Each game frame. player_crashed: the player's cycle crashed this frame; pressed: any button was
// pressed this frame. Returns 1 on the frame the game should restart (the riders start afresh)
int game_update(game_state *g, int player_crashed, int pressed);

// While terminated: how many characters of GAME_TERMINAL_TEXT are typed by now
int game_typed_chars(const game_state *g);

// While terminated: whether the cursor shows this frame (always while typing, then flashing)
int game_cursor_on(const game_state *g);

#endif
