#ifndef __STATUS_MESSAGE_H__
#define __STATUS_MESSAGE_H__

/*
 * A one-line status message during play, typed out like the terminal screen. Pure C,
 * unit-tested on the PC.
 *
 * When something happens that the picture alone doesn't make clear (a bot crashing a long way
 * off), the machine says so: "Enemy process eliminated", typed in terminal green behind a block
 * cursor. It types quicker than the terminal screen (the game's still going: a character every
 * 2 frames, so 24 characters in 0.8 s), stays up STATUS_HOLD_FRAMES with the cursor flashing,
 * then goes.
 *
 * A new message replaces one still showing, and types from the start.
 */

#define STATUS_TYPE_SHIFT 1		 // a character every 2 frames: frames >> 1 is how many are typed
#define STATUS_HOLD_FRAMES 120	 // 2 s once typed, then it goes
#define STATUS_BLINK_SHIFT 4	 // the cursor shows for 16 frames, hides for 16: quicker than the terminal's
#define STATUS_ENEMY_ELIMINATED "Enemy process eliminated"

typedef struct
{
	const char *text;	   // 0 when there's no message
	unsigned char length;  // its characters (worked out once, when it's shown)
	unsigned short frames; // frames since it was shown
} status_message;

// No message
void status_init(status_message *s);

// Show this message (a string that lasts: a constant), typing it from the start
void status_show(status_message *s, const char *text);

// Each game frame: the typing moves on, and once it's been up long enough it goes
void status_update(status_message *s);

// 1 while there's a message on screen
int status_visible(const status_message *s);

// How many of its characters are typed by now
int status_typed_chars(const status_message *s);

// Whether the cursor shows this frame: solid while it types, then flashing
int status_cursor_on(const status_message *s);

#endif
