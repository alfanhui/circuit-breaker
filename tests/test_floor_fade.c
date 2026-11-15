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

// Built every frame, shown two frames later (at the VBlank before the frame two on is built):
// each picture shows the fog built for it
static void the_fog_shown_is_the_one_built_for_the_picture(void)
{
	floor_fade_tables t;
	int table_for[20];

	floor_fade_tables_init(&t);
	for (unsigned int frame = 0; frame < 20; ++frame)
	{
		if (frame >= 2)
			CHECK_EQ(table_for[frame - 2], floor_fade_tables_show(&t, frame - 2));
		table_for[frame] = floor_fade_tables_build(&t, frame);
	}
}

// A late frame: VDP1 shows the same picture for two fields, and nothing new is shown; then the
// next. The fog stays the late frame's, and then is the next one's, not one built since
static void a_late_frame_keeps_its_own_fog(void)
{
	floor_fade_tables t;
	int a, b, c;

	floor_fade_tables_init(&t);
	a = floor_fade_tables_build(&t, 10);
	CHECK_EQ(a, floor_fade_tables_show(&t, 10));
	b = floor_fade_tables_build(&t, 11);
	c = floor_fade_tables_build(&t, 12); // frame 12 built while 11 is still being drawn
	CHECK(b != a && c != a && c != b);	  // three tables: none on screen overwritten
	CHECK_EQ(b, floor_fade_tables_show(&t, 11));
	CHECK_EQ(c, floor_fade_tables_show(&t, 12));
}

// A frame with no rebuild shows the newest fog built before it
static void a_frame_without_a_rebuild_keeps_the_last(void)
{
	floor_fade_tables t;
	int a;

	floor_fade_tables_init(&t);
	a = floor_fade_tables_build(&t, 5);
	floor_fade_tables_build(&t, 9); // for a frame not shown yet
	CHECK_EQ(a, floor_fade_tables_show(&t, 7));
}

// Built twice in one frame (a view change and a tilt): one table, the last build in it
static void two_builds_in_a_frame_share_a_table(void)
{
	floor_fade_tables t;

	floor_fade_tables_init(&t);
	CHECK_EQ(floor_fade_tables_build(&t, 3), floor_fade_tables_build(&t, 3));
}

// The table on screen is never built into, even when it's the oldest
static void the_table_on_screen_is_never_overwritten(void)
{
	floor_fade_tables t;
	int on_screen;

	floor_fade_tables_init(&t);
	on_screen = floor_fade_tables_build(&t, 1);
	floor_fade_tables_show(&t, 1);
	for (unsigned int frame = 2; frame < 10; ++frame)
		CHECK(floor_fade_tables_build(&t, frame) != on_screen);
}

// Frame numbers wrap round 2^32: still the newest
static void the_frame_numbers_can_wrap(void)
{
	floor_fade_tables t;
	int a, b;

	floor_fade_tables_init(&t);
	a = floor_fade_tables_build(&t, 0xFFFFFFFFu);
	b = floor_fade_tables_build(&t, 0u);
	CHECK_EQ(a, floor_fade_tables_show(&t, 0xFFFFFFFFu));
	CHECK_EQ(b, floor_fade_tables_show(&t, 0u));
}

void run_floor_fade_tests(void)
{
	RUN(the_fog_shown_is_the_one_built_for_the_picture);
	RUN(a_late_frame_keeps_its_own_fog);
	RUN(a_frame_without_a_rebuild_keeps_the_last);
	RUN(two_builds_in_a_frame_share_a_table);
	RUN(the_table_on_screen_is_never_overwritten);
	RUN(the_frame_numbers_can_wrap);
	RUN(ratio_spans_none_to_almost_all_fog);
	RUN(near_floor_is_untouched);
	RUN(horizon_is_full_haze);
	RUN(fade_grows_towards_the_horizon);
	RUN(glow_fades_out_above_the_horizon);
	RUN(horizon_off_screen_draws_no_fade);
	RUN(tilted_camera_fades_the_top_of_the_screen);
	RUN(high_camera_fades_the_top_of_the_screen);
}
