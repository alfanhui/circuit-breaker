#include "test.h"
#include "../src/game/skyline.h"

static skyline_box box(int x_min, int x_max, int y_min, int y_max, int top)
{
	skyline_box b = {(short)x_min, (short)x_max, (short)y_min, (short)y_max, (short)top};

	return b;
}

// The eye at the origin, 35 up, looking north (-y). A tower 500 square and 400 tall, 300 ahead
static void tower_ahead(skyline *s)
{
	const skyline_box tall = box(-250, 250, -800, -300, 400);

	skyline_begin(s, 0, 0, 35, 0, -1);
	skyline_offer(s, &tall);
}

static void a_tower_hides_what_stands_behind_it(void)
{
	skyline s;

	tower_ahead(&s);
	CHECK_EQ(1, s.count);
	// Straight behind it, the same size and no taller: gone
	CHECK(skyline_hides(&s, &(skyline_box){-250, 250, -1500, -1000, 400}));
	// Its shadow widens with distance: at y -1200 it's 4 x 250 = 1000 each way
	CHECK(skyline_hides(&s, &(skyline_box){-900, 900, -1700, -1200, 100}));
	// Wider than the shadow there: an end shows
	CHECK(!skyline_hides(&s, &(skyline_box){-1100, 900, -1700, -1200, 100}));
	// Taller than it, by the level rule: its top shows over it. (A little taller and further off
	// it's still hidden, its top seen at a lower angle: a_near_tower_hides_a_far_taller_one)
	CHECK(!skyline_hides_level(&s, &(skyline_box){-250, 250, -1500, -1000, 401}));
	CHECK(!skyline_hides(&s, &(skyline_box){-250, 250, -1500, -1000, 700}));
}

static void it_does_not_hide_itself_or_what_is_in_front(void)
{
	skyline s;

	tower_ahead(&s);
	CHECK(!skyline_hides(&s, &(skyline_box){-250, 250, -800, -300, 400})); // itself
	CHECK(!skyline_hides(&s, &(skyline_box){-50, 50, -200, -100, 10}));	   // between you and it
	CHECK(!skyline_hides(&s, &(skyline_box){400, 900, -800, -300, 10}));   // beside it
	CHECK(!skyline_hides(&s, &(skyline_box){-250, 250, 300, 800, 10}));	   // behind you
}

// Off a corner of the hider, its silhouette is its diagonal: what's behind the two sides you see
static void seen_from_off_a_corner(void)
{
	skyline s;
	const skyline_box tall = box(300, 800, -800, -300, 400);

	skyline_begin(&s, 0, 0, 35, 1, -1);
	skyline_offer(&s, &tall);
	CHECK_EQ(1, s.count);
	CHECK(skyline_hides(&s, &(skyline_box){1000, 1300, -1300, -1000, 0}));
	// Touching its far corner, but reaching out past its wedge
	CHECK(!skyline_hides(&s, &(skyline_box){800, 1300, -300, 200, 0}));
	CHECK(!skyline_hides(&s, &tall));
}

static void what_you_see_over_hides_nothing(void)
{
	skyline s;
	const skyline_box low = box(-250, 250, -800, -300, 20);
	const skyline_box behind_you = box(-250, 250, 300, 800, 400);
	const skyline_box under_you = box(-250, 250, -250, 250, 400);

	skyline_begin(&s, 0, 0, 35, 0, -1);
	skyline_offer(&s, &low); // its top's below the eye
	skyline_offer(&s, &behind_you);
	skyline_offer(&s, &under_you); // the eye's over it
	CHECK_EQ(0, s.count);
	CHECK(!skyline_hides(&s, &(skyline_box){-250, 250, -1500, -1000, 0}));
}

// More offered than there's room for: the nearest are the ones kept
static void the_nearest_hiders_are_kept(void)
{
	skyline s;

	skyline_begin(&s, 0, 0, 35, 0, -1);
	for (int i = SKYLINE_HIDERS + 3; i >= 0; --i)
	{
		// A row of towers going away to the north-east, the furthest offered first
		const skyline_box b = box(600 * i + 300, 600 * i + 800, -5000 + -100 * i, -4500 + -100 * i, 400);

		skyline_offer(&s, &b);
	}
	CHECK_EQ(SKYLINE_HIDERS, s.count);
	for (int i = 0; i < s.count; ++i)
		CHECK(s.hiders[i].reach <= 4500 + 100 * (SKYLINE_HIDERS - 1) - (400 - 35));
}

// A low tower beside you is a worse hider than a tall one further off: with room for only so
// many, the tall one's kept
static void tall_beats_near(void)
{
	skyline s;
	const skyline_box tall = box(-250, 250, -1500, -1000, 1800);

	skyline_begin(&s, 0, 0, 35, 0, -1);
	for (int i = 0; i < SKYLINE_HIDERS; ++i)
	{
		const skyline_box low = box(-2000 + 500 * i, -1600 + 500 * i, -500, -100, 40);

		skyline_offer(&s, &low);
	}
	skyline_offer(&s, &tall);
	CHECK(skyline_hides(&s, &(skyline_box){-250, 250, -2500, -2000, 400}));
}

// A near tower hides a far one taller than itself, if the far one's top is seen at a lower
// angle: 400 tall 300 off hides 2000 tall 5000 off, but not 4000 tall 1000 off. And by the
// level rule alone (skyline_hides_level), neither
static void a_near_tower_hides_a_far_taller_one(void)
{
	skyline s;

	tower_ahead(&s); // 400 tall, from 300 to 800 ahead, the eye 35 up
	// Its top's (400 - 35) over at most 800 + 125 away: 0.39 up. The far one: 1965 over 5000: 0.39
	CHECK(skyline_hides(&s, &(skyline_box){-250, 250, -5600, -5100, 2000}));
	CHECK(!skyline_hides_level(&s, &(skyline_box){-250, 250, -5600, -5100, 2000}));
	CHECK(!skyline_hides(&s, &(skyline_box){-250, 250, -1500, -1000, 4000}));
	// Still only what's in its shadow on the plan
	CHECK(!skyline_hides(&s, &(skyline_box){-6000, 6000, -5600, -5100, 2000}));
}

// A side that's partly behind a hider is cut back to what shows of it (skyline_trim)
static void a_side_is_cut_back_to_what_shows(void)
{
	skyline s;
	int from, to;

	tower_ahead(&s); // from 300 to 800 ahead, 500 wide, 400 tall: at y -1200 its shadow's x -1000 to 1000
	// One end behind it: that end comes along to the shadow's edge, a little short of it
	from = -500;
	to = 2000;
	CHECK(skyline_trim(&s, true, -1200, &from, &to, 300));
	CHECK(from <= 1000 && from >= 1000 - (2500 >> SKYLINE_TRIM_STEPS) - 1);
	CHECK_EQ(2000, to);
	from = -2000;
	to = 500;
	CHECK(skyline_trim(&s, true, -1200, &from, &to, 300));
	CHECK_EQ(-2000, from);
	CHECK(to >= -1000 && to <= -1000 + (2500 >> SKYLINE_TRIM_STEPS) + 1);
	// All of it behind: nothing to draw
	from = -900;
	to = 900;
	CHECK(!skyline_trim(&s, true, -1200, &from, &to, 300));
	// Across the shadow, both ends showing: left alone
	from = -1500;
	to = 1500;
	CHECK(skyline_trim(&s, true, -1200, &from, &to, 300));
	CHECK_EQ(-1500, from);
	CHECK_EQ(1500, to);
	// Taller than the hider: its top shows over it, so all of it's drawn
	from = -500;
	to = 2000;
	CHECK(skyline_trim(&s, true, -1200, &from, &to, 401));
	CHECK_EQ(-500, from);
	// In front of the hider, and beside it: untouched
	from = -100;
	to = 100;
	CHECK(skyline_trim(&s, true, -200, &from, &to, 100));
	CHECK_EQ(-100, from);
	CHECK_EQ(100, to);
	// A side running away from you (along y, at x 100), its near end in front of the hider...
	from = -2000;
	to = -1000;
	CHECK(!skyline_trim(&s, false, 100, &from, &to, 300)); // ...no: all of this one's behind it
	// The hider's own front isn't in its own shadow
	from = -250;
	to = 250;
	CHECK(skyline_trim(&s, true, -300, &from, &to, 400));
	CHECK_EQ(-250, from);
	CHECK_EQ(250, to);
}

void run_skyline_tests(void)
{
	RUN(a_side_is_cut_back_to_what_shows);
	RUN(a_near_tower_hides_a_far_taller_one);
	RUN(tall_beats_near);
	RUN(a_tower_hides_what_stands_behind_it);
	RUN(it_does_not_hide_itself_or_what_is_in_front);
	RUN(seen_from_off_a_corner);
	RUN(what_you_see_over_hides_nothing);
	RUN(the_nearest_hiders_are_kept);
}
