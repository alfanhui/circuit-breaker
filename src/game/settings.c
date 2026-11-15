#include "settings.h"
#include "../features.h" // the defaults: SHOW_DEBUG_TEXT, SFX_ON, CYCLE_SHAPE ...

settings game_settings;

// Each switch's name on the menu, and how many values it has (2: off / on)
static const char *const names[SETTING_COUNT] = {
	"Debug text", "Flight text", "Sound effects", "Cycle shape", "City sides", "No termination",
};
static const unsigned char choices[SETTING_COUNT] = {2, 2, 2, 4, 2, 2};
static const char *const off_on[2] = {"off", "on"};
static const char *const shapes[4] = {"ribbon", "token", "cursor", "linked"}; // CYCLE_RIBBON ... CYCLE_LINKED

void settings_init(settings *s)
{
	s->value[SETTING_DEBUG_TEXT] = SHOW_DEBUG_TEXT ? 1 : 0;
	s->value[SETTING_FLIGHT_TEXT] = SHOW_FLIGHT_DEBUG ? 1 : 0;
	s->value[SETTING_SFX] = SFX_ON ? 1 : 0;
	s->value[SETTING_CYCLE_SHAPE] = CYCLE_SHAPE;
	s->value[SETTING_CITY_SIDES] = CITY_SIDES_TO_BOARD ? 1 : 0;
	s->value[SETTING_NO_TERMINATION] = 0; // always on at boot: a crash terminates you
	s->cursor = 0;
}

void settings_next(settings *s)
{
	// Round without a remainder (%: a division on the SH-2)
	if (++s->cursor >= SETTING_COUNT)
		s->cursor = 0;
}

void settings_change_one(settings *s, setting which)
{
	if (++s->value[which] >= choices[which])
		s->value[which] = 0;
}

setting settings_change(settings *s)
{
	const setting which = (setting)s->cursor;

	settings_change_one(s, which);
	return which;
}

void settings_line(const settings *s, setting which, char *out)
{
	const char *name = names[which];
	const char *value = which == SETTING_CYCLE_SHAPE ? shapes[s->value[which] & 3] : off_on[s->value[which] ? 1 : 0];
	int length = 0, at = 0;

	while (value[length])
		length++;
	out[0] = s->cursor == which ? '>' : ' ';
	out[1] = ' ';
	at = 2;
	while (*name)
		out[at++] = *name++;
	out[at++] = ' ';
	// Dots up to a space before the value, which ends the line
	while (at < SETTINGS_LINE_LENGTH - length - 1)
		out[at++] = '.';
	out[at++] = ' ';
	while (*value)
		out[at++] = *value++;
	out[at] = 0;
}
