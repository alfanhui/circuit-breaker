#include <string.h>
#include "test.h"
#include "../src/game/settings.h"
#include "../src/features.h"

static void starts_as_features_h_says(void)
{
	settings s;

	settings_init(&s);
	CHECK_EQ(SHOW_DEBUG_TEXT ? 1 : 0, settings_value(&s, SETTING_DEBUG_TEXT));
	CHECK_EQ(SFX_ON ? 1 : 0, settings_value(&s, SETTING_SFX));
	CHECK_EQ(CYCLE_SHAPE, settings_value(&s, SETTING_CYCLE_SHAPE));
	CHECK_EQ(0, settings_value(&s, SETTING_NO_TERMINATION)); // a crash terminates you, at boot
	CHECK_EQ(0, s.cursor);
}

// L: down the list, and round from the last to the first
static void l_goes_round_the_list(void)
{
	settings s;

	settings_init(&s);
	for (int i = 1; i < SETTING_COUNT; ++i)
	{
		settings_next(&s);
		CHECK_EQ(i, s.cursor);
	}
	settings_next(&s);
	CHECK_EQ(0, s.cursor);
}

// R: off -> on -> off, only the one under the cursor
static void r_flips_the_one_under_the_cursor(void)
{
	settings s;

	settings_init(&s);
	s.value[SETTING_SFX] = 1;
	s.value[SETTING_DEBUG_TEXT] = 1;
	s.cursor = SETTING_SFX;
	CHECK_EQ(SETTING_SFX, settings_change(&s));
	CHECK_EQ(0, settings_value(&s, SETTING_SFX));
	CHECK_EQ(1, settings_value(&s, SETTING_DEBUG_TEXT)); // the others as they were
	settings_change(&s);
	CHECK_EQ(1, settings_value(&s, SETTING_SFX));
}

// The shape steps through all four, then back to the first
static void the_shape_goes_through_four(void)
{
	settings s;

	settings_init(&s);
	s.value[SETTING_CYCLE_SHAPE] = 0;
	s.cursor = SETTING_CYCLE_SHAPE;
	for (int i = 1; i < 4; ++i)
	{
		settings_change(&s);
		CHECK_EQ(i, settings_value(&s, SETTING_CYCLE_SHAPE));
	}
	settings_change(&s);
	CHECK_EQ(0, settings_value(&s, SETTING_CYCLE_SHAPE));
}

// Z flips no termination wherever the cursor is
static void one_flipped_away_from_the_cursor(void)
{
	settings s;

	settings_init(&s);
	settings_change_one(&s, SETTING_NO_TERMINATION);
	CHECK(settings_on(&s, SETTING_NO_TERMINATION));
	CHECK_EQ(0, s.cursor);
}

// Every line the same length (it covers the last one printed there), the cursor's marked
static void lines_are_all_one_length(void)
{
	settings s;
	char line[SETTINGS_LINE_LENGTH + 1];

	settings_init(&s);
	s.value[SETTING_SFX] = 1;
	s.cursor = SETTING_SFX;
	settings_line(&s, SETTING_SFX, line);
	CHECK(strcmp(line, "> Sound effects ....... on") == 0);
	settings_line(&s, SETTING_DEBUG_TEXT, line);
	CHECK_EQ(' ', line[0]);
	for (int which = 0; which < SETTING_COUNT; ++which)
		for (int value = 0; value < 4; ++value)
		{
			s.value[which] = (unsigned char)(which == SETTING_CYCLE_SHAPE ? value : value & 1);
			settings_line(&s, (setting)which, line);
			CHECK_EQ(SETTINGS_LINE_LENGTH, (int)strlen(line));
		}
	s.value[SETTING_CYCLE_SHAPE] = 1;
	settings_line(&s, SETTING_CYCLE_SHAPE, line);
	CHECK(strcmp(line + SETTINGS_LINE_LENGTH - 5, "token") == 0);
}

void run_settings_tests(void)
{
	starts_as_features_h_says();
	l_goes_round_the_list();
	r_flips_the_one_under_the_cursor();
	the_shape_goes_through_four();
	one_flipped_away_from_the_cursor();
	lines_are_all_one_length();
}
