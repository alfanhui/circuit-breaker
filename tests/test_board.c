#include "test.h"
#include "../src/game/board.h"

/*
 * The circuit board under the city (game/board.h): its pulses, and that FLOOR.TGA, made by
 * tools/make_floor.py, holds the arena's grid and the board the way board.c reads it
 */

// A pulse's head is on phase `step`, its tail on the phases behind it; the rest are plain trace
static void a_pulse_runs_along_the_phases(void)
{
	const unsigned short trace = board_colour(BOARD_FIRST_PHASE + 10, 0);

	CHECK(board_colour(BOARD_FIRST_PHASE + 5, 5) != trace);					   // the head
	CHECK(board_colour(BOARD_FIRST_PHASE + 4, 5) != trace);					   // behind it
	CHECK(board_colour(BOARD_FIRST_PHASE + 3, 5) != trace);					   // its tail's end
	CHECK_EQ(trace, board_colour(BOARD_FIRST_PHASE + 2, 5));				   // past the tail
	CHECK_EQ(trace, board_colour(BOARD_FIRST_PHASE + 6, 5));				   // ahead of the head
	CHECK_EQ(board_colour(BOARD_FIRST_PHASE + 5, 5), board_colour(BOARD_FIRST_PHASE + 6, 6)); // a step on, a phase on
	// The head's the brightest part: more green than the next
	CHECK(((board_colour(BOARD_FIRST_PHASE + 5, 5) >> 5) & 31) > ((board_colour(BOARD_FIRST_PHASE + 4, 5) >> 5) & 31));
}

// One pulse every BOARD_PULSE_PERIOD phases, and the step wraps round with no jump
static void pulses_repeat_and_wrap(void)
{
	for (unsigned int step = 0; step < 3 * BOARD_PHASES; ++step)
	{
		int lit = 0;

		for (int p = 0; p < BOARD_PHASES; ++p)
			lit += board_colour(BOARD_FIRST_PHASE + p, step) != board_colour(BOARD_FIRST_PHASE + p, step + BOARD_PULSE_PERIOD / 2) ? 1 : 0;
		CHECK_EQ(2 * BOARD_PULSE_LENGTH * (BOARD_PHASES / BOARD_PULSE_PERIOD), lit);
	}
	CHECK_EQ(board_colour(BOARD_FIRST_PHASE, 0xFFFFFFFFu + 1), board_colour(BOARD_FIRST_PHASE, 0));
	CHECK_EQ(board_colour(BOARD_FIRST_PHASE + 31, 0xFFFFFFFFu), board_colour(BOARD_FIRST_PHASE + 15, 15));
}

// The palette gives every grid shade of a class that class's colour, and stops at BOARD_INDICES
static void the_palette_goes_by_the_class(void)
{
	unsigned short palette[256];

	for (int i = 0; i < 256; ++i)
		palette[i] = 0x1234;
	board_palette(palette, 7);
	for (int i = 0; i < BOARD_INDICES; ++i)
	{
		int c = i / BOARD_GRID_SHADES;

		CHECK_EQ(board_colour(c, 7), palette[i]);
		CHECK(palette[i] & 0x8000); // a colour VDP2 shows
	}
	CHECK_EQ(0x1234, palette[BOARD_INDICES]); // left alone past the board's
}

// FLOOR.TGA: its palette's the arena's grid (entry i is grid shade i % BOARD_GRID_SHADES, so the
// arena doesn't see the board), every texel's index is the board's, and every class is in it
static void the_floor_tile_holds_both(void)
{
	static unsigned char file[18 + 256 * 3 + 256 * 256];
	FILE *f = fopen("../cd/FLOOR.TGA", "rb");
	size_t got;
	int used[BOARD_CLASSES] = {0};

	CHECK(f != NULL);
	if (f == NULL)
		return;
	got = fread(file, 1, sizeof(file), f);
	fclose(f);
	CHECK_EQ(sizeof(file), got);
	CHECK_EQ(256, file[5] | (file[6] << 8)); // the palette's 256 entries
	CHECK_EQ(256, file[12] | (file[13] << 8));
	for (int i = 0; i < 256; ++i)
		for (int k = 0; k < 3; ++k)
			CHECK_EQ(file[18 + (i % BOARD_GRID_SHADES) * 3 + k], file[18 + i * 3 + k]);
	for (int t = 0; t < 256 * 256; ++t)
	{
		const int index = file[18 + 256 * 3 + t];

		CHECK(index < BOARD_INDICES);
		if (index < BOARD_INDICES)
			used[index / BOARD_GRID_SHADES] = 1;
	}
	for (int c = 0; c < BOARD_CLASSES; ++c)
		CHECK(used[c]);
}

void run_board_tests(void)
{
	RUN(a_pulse_runs_along_the_phases);
	RUN(pulses_repeat_and_wrap);
	RUN(the_palette_goes_by_the_class);
	RUN(the_floor_tile_holds_both);
}
