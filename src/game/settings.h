#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdbool.h>

/*
 * The switches you can change while playing, from the pause menu (START): L moves to the next
 * one, R changes it. Pure C, unit-tested on the PC; input.c steers it, main.c prints it.
 *
 * Each starts as src/features.h says (its default, built in); the menu changes it until the
 * game's switched off. Only switches that cost nothing to change live are here: the sky and the
 * two fogs set the VDPs up once at boot (an image loaded, tables made, quads given Gouraud), so
 * they stay build switches, and so do the boot ones (by the time you can pause, you've booted).
 *
 * A switch is a small number: 0 / 1 for off / on, or which of a few (the cycle's shape, 0 to 3).
 */

typedef enum
{
	SETTING_DEBUG_TEXT,		// the debug readout, top left (main.c, debug_3d)
	SETTING_FLIGHT_TEXT,	// the opening flight's debug lines (main.c, debug_flight)
	SETTING_SFX,			// the sound effects (sound.h)
	SETTING_CYCLE_SHAPE,	// ribbon / token / cursor / linked (features.h, CYCLE_SHAPE)
	SETTING_CITY_SIDES,		// the city's tower sides down to the board (level.c)
	SETTING_NO_TERMINATION, // a crash only stops the cycle (also Z while paused)
	SETTING_COUNT
} setting;

// The menu: every switch's value, and the one the cursor's on
typedef struct
{
	unsigned char value[SETTING_COUNT];
	unsigned char cursor;
} settings;

// The game's own (input.c sets them up at boot; anything may read them)
extern settings game_settings;

// Every switch as features.h has it, the cursor on the first
void settings_init(settings *s);

// L: the cursor to the next switch, round from the last to the first
void settings_next(settings *s);

// R: the switch under the cursor to its next value (off -> on -> off; a shape to the next one).
// Returns which switch changed, for anything that has to follow it (a new cycle model)
setting settings_change(settings *s);

// That one switch to its next value, wherever the cursor is (Z: no termination)
void settings_change_one(settings *s, setting which);

// Its value: 0 is off, for an on / off one
static inline int settings_value(const settings *s, setting which)
{
	return s->value[which];
}
static inline bool settings_on(const settings *s, setting which)
{
	return s->value[which] != 0;
}

#define SETTINGS_LINE_LENGTH 26 // each menu line's characters (they're printed centred on 40)

/*
 * One menu line, for the text layer: a mark if the cursor's on it, the switch's name, dots, its
 * value, always SETTINGS_LINE_LENGTH long (so it covers whatever was printed there before):
 *     "> Sound effects ...... on"
 * out: room for SETTINGS_LINE_LENGTH + 1 (the 0 at the end)
 */
void settings_line(const settings *s, setting which, char *out);

#endif
