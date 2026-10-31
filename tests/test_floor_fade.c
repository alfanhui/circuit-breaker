#include "test.h"
#include "../src/game/floor_fade.h"

#define ROWS 224

// The 1st person view: horizon at mid-screen, eye 35 up, 90 degree FOV on a 320 wide screen
static const floor_fade_view first_person = {
	.centre_row = 112, .tilt = 0, .eye_height = 35, .focal = 160,
	.fade_start = 700, .fade_end = 4000, .glow_rows = 6};

// The iso view: 768 up, looking 50 degrees down (tan = 1.192), 60 degree FOV (focal 160 / tan 30 = 277)
static const floor_fade_view iso = {
	.centre_row = 112, .tilt = 1192, .eye_height = 768, .focal = 277,
	.fade_start = 700, .fade_end = 1600, .glow_rows = 0};

static unsigned char levels[ROWS];

static void near_floor_is_untouched(void)
{
	floor_fade_rows(&first_person, levels, ROWS);
	// Row 120 shows the floor 35 * 160 / 8 = 700 away: the start of the fade
	for (int row = 120; row < ROWS; ++row)
		CHECK_EQ(0, levels[row]);
}

static void horizon_is_full_haze(void)
{
	floor_fade_rows(&first_person, levels, ROWS);
	CHECK_EQ(FLOOR_FADE_LEVELS - 1, levels[112]); // the horizon itself
	CHECK_EQ(FLOOR_FADE_LEVELS - 1, levels[113]); // 5600 away: past fade_end
}

static void fade_grows_towards_the_horizon(void)
{
	floor_fade_rows(&first_person, levels, ROWS);
	for (int row = 113; row < ROWS - 1; ++row)
		CHECK(levels[row] >= levels[row + 1]); // never lighter further from the horizon
	CHECK(levels[116] > 0 && levels[116] < FLOOR_FADE_LEVELS - 1); // 1400 away: part way
}

static void glow_fades_out_above_the_horizon(void)
{
	floor_fade_rows(&first_person, levels, ROWS);
	for (int row = 112 - 6 + 1; row < 112; ++row)
		CHECK(levels[row] <= levels[row + 1]); // dimmer the higher it goes
	CHECK(levels[111] > 0);
	CHECK_EQ(0, levels[112 - 6]);
	CHECK_EQ(0, levels[0]);
}

static void horizon_off_screen_draws_no_fade(void)
{
	floor_fade_view high = first_person;
	high.centre_row = -100; // the horizon 100 rows above the top of the screen
	high.glow_rows = 0;
	floor_fade_rows(&high, levels, ROWS);
	CHECK_EQ(0, levels[0]); // row 0 is 100 rows below the horizon: 35 * 160 / 100 = 56 away
}

static void tilted_camera_fades_the_top_of_the_screen(void)
{
	floor_fade_rows(&iso, levels, ROWS);
	// Top row: 768 * (277000 + 1192 * 112) / (1192 * 277 - 112000) = 1444 away: most of the way
	CHECK_EQ((1444 - 700) * (FLOOR_FADE_LEVELS - 1) / (1600 - 700), levels[0]);
	CHECK_EQ(0, levels[112]);	// the centre: 768 / tan 50 = 644 away, before the fade
	CHECK_EQ(0, levels[ROWS - 1]); // bottom: ~250 away
	for (int row = 0; row < ROWS - 1; ++row)
		CHECK(levels[row] >= levels[row + 1]); // fades more towards the top
}

// The spectator view (floor_fade_vdp2.c): 3804 up, 25 degrees down, a 30 degree FOV. Its
// eye_height x focal x 1000 is past what 32 bits hold (3804 x 597000 = 2.27 billion): worked
// out that way it wrapped negative, read as "under the camera", and nothing was fogged
static void high_camera_fades_the_top_of_the_screen(void)
{
	static const floor_fade_view spectator = {
		.centre_row = 112, .tilt = 466, .eye_height = 3804, .focal = 597,
		.fade_start = 9700, .fade_end = 20000, .glow_rows = 0};

	floor_fade_rows(&spectator, levels, ROWS);
	// Top row: 3804 * (597000 + 466 * 112) / (466 * 597 - 112000) = 14700 away: about half way
	CHECK(levels[0] >= 6 && levels[0] <= 8);
	CHECK_EQ(0, levels[112]);	   // the centre: 8157 away, before the fade
	CHECK_EQ(0, levels[ROWS - 1]); // bottom: nearer still
	for (int row = 0; row < ROWS - 1; ++row)
		CHECK(levels[row] >= levels[row + 1]); // fades more towards the top
}

static void ratio_spans_none_to_almost_all_fog(void)
{
	CHECK_EQ(0, floor_fade_ratio(0)); // near floor: untouched
	CHECK_EQ(FLOOR_FADE_RATIO_MAX, floor_fade_ratio(FLOOR_FADE_LEVELS - 1)); // horizon: 1/32 floor
	CHECK_EQ(FLOOR_FADE_RATIO_MAX, floor_fade_ratio(FLOOR_FADE_LEVELS + 5)); // clamped
	CHECK_EQ(0, floor_fade_ratio(-1));
	CHECK_EQ(2, floor_fade_ratio(1)); // 31 / 15 = 2.07, rounded
	for (int level = 1; level < FLOOR_FADE_LEVELS; ++level)
		CHECK(floor_fade_ratio(level) > floor_fade_ratio(level - 1)); // every level a little darker
}

void run_floor_fade_tests(void)
{
	RUN(ratio_spans_none_to_almost_all_fog);
	RUN(near_floor_is_untouched);
	RUN(horizon_is_full_haze);
	RUN(fade_grows_towards_the_horizon);
	RUN(glow_fades_out_above_the_horizon);
	RUN(horizon_off_screen_draws_no_fade);
	RUN(tilted_camera_fades_the_top_of_the_screen);
	RUN(high_camera_fades_the_top_of_the_screen);
}
