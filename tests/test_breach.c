#include "test.h"
#include "../src/game/breach.h"

// Runs frames, returns the phase it's in after
static int run_frames(breach *b, int frames)
{
	for (int i = 0; i < frames; ++i)
		breach_update(b);
	return b->phase;
}

static int glitched(const breach_glitch *g)
{
	return (g->shift_x | g->shift_y | g->red | g->green | g->blue | g->band_count | g->reset_shows) != 0;
}

static void nothing_until_it_starts(void)
{
	breach b;

	breach_init(&b);
	CHECK_EQ(BREACH_NONE, run_frames(&b, 500));
	CHECK(!glitched(&b.glitch));
	CHECK_EQ(0, breach_roll(&b));
	CHECK_EQ(0, breach_dip(&b));
	CHECK_EQ(0, breach_drop(&b));
	CHECK_EQ(0, breach_distance(&b, 7));
}

// The tear, through the hole, the fall, the skid, then lying still for good
static void the_parts_follow_on(void)
{
	breach b;

	breach_start(&b);
	CHECK_EQ(BREACH_TEAR, run_frames(&b, BREACH_TEAR_FRAMES - 1));
	CHECK_EQ(BREACH_THROUGH, run_frames(&b, 1));
	CHECK_EQ(BREACH_THROUGH, run_frames(&b, BREACH_THROUGH_FRAMES - 1));
	CHECK_EQ(BREACH_FALL, run_frames(&b, 1));
	CHECK_EQ(BREACH_FALL, run_frames(&b, BREACH_FALL_FRAMES - 1));
	CHECK_EQ(BREACH_SKID, run_frames(&b, 1));
	CHECK(breach_landed(&b));
	CHECK_EQ(BREACH_SKID, run_frames(&b, BREACH_SKID_FRAMES - 1));
	CHECK(!breach_landed(&b));
	CHECK_EQ(BREACH_STOPPED, run_frames(&b, 1));
	CHECK_EQ(BREACH_STOPPED, run_frames(&b, 70000)); // however long
	// Until a button gets you up, and you rise for its frames, then you're on your feet for good
	breach_get_up(&b);
	CHECK_EQ(BREACH_RISE, b.phase);
	CHECK_EQ(BREACH_RISE, run_frames(&b, BREACH_RISE_FRAMES - 1));
	CHECK_EQ(BREACH_ON_FOOT, run_frames(&b, 1));
	CHECK_EQ(BREACH_ON_FOOT, run_frames(&b, 70000));
	breach_get_up(&b);
	CHECK_EQ(BREACH_ON_FOOT, b.phase); // and up only once
}

// A button only gets you up once you're lying still: a press made riding in or skidding does
// nothing. One made lying there, but before you've lain there a moment, is kept: up once it's
// passed
static void you_get_up_only_once_lying_still(void)
{
	breach b;

	breach_init(&b);
	breach_get_up(&b);
	CHECK_EQ(BREACH_NONE, b.phase);
	breach_start(&b);
	for (int i = 0; i < BREACH_TEAR_FRAMES + BREACH_THROUGH_FRAMES + BREACH_FALL_FRAMES + BREACH_SKID_FRAMES - 1; ++i)
	{
		breach_get_up(&b);
		CHECK(b.phase != BREACH_RISE);
		breach_update(&b);
	}
	breach_update(&b);
	CHECK_EQ(BREACH_STOPPED, b.phase);
	CHECK(!b.wants_up); // the presses riding in weren't kept
	run_frames(&b, BREACH_LIE_FRAMES - 1);
	CHECK_EQ(BREACH_STOPPED, b.phase); // nothing pressed: lying there
	breach_get_up(&b); // pressed a frame too soon
	CHECK_EQ(BREACH_STOPPED, b.phase);
	run_frames(&b, 1); // the last frame of lying there
	CHECK_EQ(BREACH_STOPPED, b.phase);
	run_frames(&b, 1); // and up
	CHECK_EQ(BREACH_RISE, b.phase);

	// Lain there long enough: up at once
	breach_start(&b);
	while (b.phase != BREACH_STOPPED)
		breach_update(&b);
	run_frames(&b, BREACH_LIE_FRAMES);
	breach_get_up(&b);
	CHECK_EQ(BREACH_RISE, b.phase);
}

// Getting up takes the tilt and the drop from where you lay to nothing: never more than lying,
// never less than 0, a little less each frame, and level at the end. It begins slowly
static void getting_up_levels_out(void)
{
	breach b;
	int roll = BREACH_REST_ROLL, dip = BREACH_REST_DIP, drop = BREACH_DROP;

	breach_start(&b);
	while (b.phase != BREACH_STOPPED)
		breach_update(&b);
	run_frames(&b, BREACH_LIE_FRAMES);
	breach_get_up(&b);
	CHECK_EQ(BREACH_REST_ROLL, breach_roll(&b));
	CHECK_EQ(BREACH_REST_DIP, breach_dip(&b));
	CHECK_EQ(BREACH_DROP, breach_drop(&b));
	breach_update(&b);
	CHECK(breach_dip(&b) > BREACH_REST_DIP - 20); // eased in: the first frame hardly moves
	while (b.phase == BREACH_RISE)
	{
		CHECK(breach_roll(&b) <= roll && breach_roll(&b) >= 0);
		CHECK(breach_dip(&b) <= dip && breach_dip(&b) >= 0);
		CHECK(breach_drop(&b) <= drop && breach_drop(&b) >= 0);
		CHECK(!glitched(&b.glitch));
		roll = breach_roll(&b);
		dip = breach_dip(&b);
		drop = breach_drop(&b);
		breach_update(&b);
	}
	CHECK_EQ(BREACH_ON_FOOT, b.phase);
	CHECK_EQ(0, breach_roll(&b));
	CHECK_EQ(0, breach_dip(&b));
	CHECK_EQ(0, breach_drop(&b));
	CHECK_EQ(0, breach_distance(&b, 20));
	CHECK_EQ(0, breach_rumble(&b));
}

// The hit itself always glitches, and the tear gets worse: more glitched frames in its last
// third than its first
static void the_tear_gets_worse(void)
{
	breach b;
	int first = 0, last = 0;

	breach_start(&b);
	breach_update(&b);
	CHECK(glitched(&b.glitch));
	for (int i = 1; i < BREACH_TEAR_FRAMES; ++i)
	{
		breach_update(&b);
		if (i < 24)
			first += glitched(&b.glitch);
		else if (i >= 48)
			last += glitched(&b.glitch);
	}
	CHECK(first > 0);
	CHECK(last > first);
}

// Every glitch stays on the screen: jumps within their range, bands inside the 224 rows,
// colour offsets inside VDP2's -255..255. Nothing glitches once it's going down: it doesn't fade
static void glitches_stay_in_range(void)
{
	breach b;
	int resets = 0;

	breach_start(&b);
	for (int i = 0; i < BREACH_TEAR_FRAMES + BREACH_THROUGH_FRAMES + BREACH_FALL_FRAMES + BREACH_SKID_FRAMES + 60;
		 ++i)
	{
		const breach_glitch *g = &b.glitch;

		breach_update(&b);
		CHECK(g->shift_x >= -BREACH_SHIFT_X_MAX && g->shift_x < BREACH_SHIFT_X_MAX);
		CHECK(g->shift_y >= -BREACH_SHIFT_Y_MAX && g->shift_y < BREACH_SHIFT_Y_MAX);
		CHECK(g->band_count <= BREACH_BANDS_MAX);
		for (int k = 0; k < g->band_count; ++k)
		{
			CHECK(g->bands[k].top >= 0);
			CHECK(g->bands[k].height >= 2 && g->bands[k].height <= BREACH_BAND_HEIGHT_MAX);
			CHECK(g->bands[k].top + g->bands[k].height <= 224);
		}
		CHECK(g->red >= -255 && g->red <= 255);
		CHECK(g->green >= -255 && g->green <= 255);
		CHECK(g->blue >= -255 && g->blue <= 255);
		resets += g->reset_shows;
		CHECK(!g->reset_shows || b.phase == BREACH_TEAR); // the reset only tries during the tear
		if (b.phase >= BREACH_FALL && b.frames > 0) // (the frame it began was still the hole's)
			CHECK(!glitched(g));
	}
	CHECK(resets > 0); // it does try
}

// The reset's text never finishes: it's cut off part way
static void the_reset_never_finishes(void)
{
	breach b;

	breach_start(&b);
	run_frames(&b, BREACH_TEAR_FRAMES - 1);
	CHECK_EQ(BREACH_RESET_CHARS_MAX, breach_typed_chars(&b));
	CHECK(breach_typed_chars(&b) < (int)sizeof("SH-2 process terminated...") - 1);
}

// Frozen in the tear; at least BREACH_SPEED_MIN through the hole and down (at cruise if faster);
// then the skid slows every frame, to nothing
static void it_rides_on_then_skids_to_a_stop(void)
{
	breach b;
	int last;

	breach_start(&b);
	breach_update(&b);
	CHECK_EQ(0, breach_distance(&b, 7));
	run_frames(&b, BREACH_TEAR_FRAMES);
	CHECK_EQ(BREACH_THROUGH, b.phase);
	CHECK_EQ(BREACH_SPEED_MIN, breach_distance(&b, 7));
	CHECK_EQ(30, breach_distance(&b, 30));
	run_frames(&b, BREACH_THROUGH_FRAMES + BREACH_FALL_FRAMES);
	CHECK_EQ(BREACH_SKID, b.phase);
	last = breach_distance(&b, 20);
	CHECK(last < 20);
	CHECK(last > 15); // it's only just begun
	for (int i = 1; i < BREACH_SKID_FRAMES; ++i)
	{
		breach_update(&b);
		CHECK(breach_distance(&b, 20) <= last);
		last = breach_distance(&b, 20);
	}
	CHECK_EQ(0, last);
	breach_update(&b);
	CHECK_EQ(BREACH_STOPPED, b.phase);
	CHECK_EQ(0, breach_distance(&b, 20));
}

// The camera: down in the fall, further every frame; then settling, still wiggling, at rest by
// the end of the skid, and left there. The wiggle dies away with the motion
static void the_camera_goes_down_and_settles(void)
{
	breach b;
	int last_dip = 0, last_rumble;

	breach_start(&b);
	run_frames(&b, BREACH_TEAR_FRAMES + BREACH_THROUGH_FRAMES);
	CHECK_EQ(0, breach_dip(&b));
	for (int i = 0; i < BREACH_FALL_FRAMES - 1; ++i)
	{
		breach_update(&b);
		CHECK(breach_dip(&b) > last_dip);
		CHECK(breach_roll(&b) > 0);
		CHECK(breach_drop(&b) <= BREACH_DROP);
		CHECK_EQ(0, breach_rumble(&b));
		last_dip = breach_dip(&b);
	}
	breach_update(&b); // the skid's first frame
	CHECK_EQ(BREACH_SKID, b.phase);
	last_rumble = breach_rumble(&b);
	CHECK(last_rumble > 0);
	for (int i = 1; i < BREACH_SKID_FRAMES; ++i)
	{
		breach_update(&b);
		CHECK(breach_rumble(&b) <= last_rumble);
		last_rumble = breach_rumble(&b);
	}
	CHECK_EQ(0, last_rumble);
	CHECK_EQ(BREACH_REST_DIP, breach_dip(&b));
	CHECK_EQ(BREACH_REST_ROLL, breach_roll(&b));
	run_frames(&b, 100);
	CHECK_EQ(BREACH_REST_DIP, breach_dip(&b));
	CHECK_EQ(BREACH_REST_ROLL, breach_roll(&b));
	CHECK_EQ(BREACH_DROP, breach_drop(&b));
	CHECK_EQ(0, breach_rumble(&b));
}

// The same crash glitches the same way every time (repeatable for the E2E shots)
static void the_same_every_time(void)
{
	breach a, b;

	breach_start(&a);
	breach_start(&b);
	for (int i = 0; i < BREACH_TEAR_FRAMES; ++i)
	{
		breach_update(&a);
		breach_update(&b);
		CHECK_EQ(a.glitch.shift_x, b.glitch.shift_x);
		CHECK_EQ(a.glitch.band_count, b.glitch.band_count);
		CHECK_EQ(a.glitch.reset_shows, b.glitch.reset_shows);
	}
}

void run_breach_tests(void)
{
	RUN(nothing_until_it_starts);
	RUN(the_parts_follow_on);
	RUN(the_tear_gets_worse);
	RUN(glitches_stay_in_range);
	RUN(the_reset_never_finishes);
	RUN(it_rides_on_then_skids_to_a_stop);
	RUN(the_camera_goes_down_and_settles);
	RUN(the_same_every_time);
	RUN(you_get_up_only_once_lying_still);
	RUN(getting_up_levels_out);
}
