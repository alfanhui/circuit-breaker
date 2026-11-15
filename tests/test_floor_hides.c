// What the floor you stand on hides (game/floor_hides.h): never more than it really does, and close
// to all of it. The exact answer is worked out here with doubles: fine on the PC, never in the game
#include "test.h"
#include "../src/game/floor_hides.h"

// A roof like the city's: 500 square, at 1000, the eye 35 over it (city.h, walker.h)
#define ROOF_MIN 0
#define ROOF_MAX 500
#define ROOF_Z 1000
#define EYE_Z (ROOF_Z + 35)

// Exactly: is (x, y, z) below the floor, and the line from the eye to it through the floor?
static int truly_hidden(int ex, int ey, int ez, int x, int y, int z)
{
	double t, qx, qy;

	if (z >= ROOF_Z)
		return 0;
	t = (double)(ez - ROOF_Z) / (double)(ez - z); // where the line crosses the floor's height
	qx = ex + t * (x - ex);
	qy = ey + t * (y - ey);
	return qx > ROOF_MIN && qx < ROOF_MAX && qy > ROOF_MIN && qy < ROOF_MAX;
}

// A small, repeatable random (no rand(): the same on every PC)
static unsigned int seed = 12345;
static int random_in(int lo, int hi)
{
	seed = seed * 1103515245u + 12345u;
	return lo + (int)((seed >> 8) % (unsigned int)(hi - lo + 1));
}

static void never_hides_what_shows(void)
{
	floor_hides f;
	int checked = 0;

	for (int n = 0; n < 4000; ++n)
	{
		const int ex = random_in(ROOF_MIN + 1, ROOF_MAX - 1), ey = random_in(ROOF_MIN + 1, ROOF_MAX - 1);
		const int x = random_in(-3000, 3500), y = random_in(-3000, 3500);
		int below;

		floor_hides_begin(&f, ex, ey, EYE_Z, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
		below = floor_hides_below(&f, x, y);
		if (below == FLOOR_HIDES_NOTHING)
			continue;
		// Every height it calls hidden really is, down to the street (-3000)
		for (int z = below; z > -3000; z -= 97)
		{
			if (!truly_hidden(ex, ey, EYE_Z, x, y, z))
			{
				CHECK(0); // it would cut away something you can see
				return;
			}
			++checked;
		}
	}
	CHECK(checked > 10000); // (and it did call a good many hidden)
}

// Close to the exact height: standing near the west edge, a tower's side across the street east
static void hides_nearly_all_it_could(void)
{
	floor_hides f;
	int below, exact = ROOF_Z;

	floor_hides_begin(&f, 40, 250, EYE_Z, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
	below = floor_hides_below(&f, 700, 250); // 200 past the east edge
	while (exact > -3000 && !truly_hidden(40, 250, EYE_Z, 700, 250, exact - 1))
		--exact; // the exact highest hidden height
	CHECK(below <= exact);
	CHECK(below > exact - 20); // within 20 units
	// The old rule (the nearest edge's distance, 40, every way) hid only below ~1000 - 35 * 700/40:
	// so far down it changed nothing. This way hides the side up to ~990
	CHECK(below > ROOF_Z - 30);
}

static void straight_below_the_floor_is_hidden_below_it(void)
{
	floor_hides f;

	floor_hides_begin(&f, 250, 250, EYE_Z, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
	CHECK_EQ(ROOF_Z - 1, floor_hides_below(&f, 250, 250));
	CHECK_EQ(ROOF_Z - 1, floor_hides_below(&f, 100, 400)); // anywhere under the floor
}

static void off_the_floor_hides_nothing(void)
{
	floor_hides f;

	floor_hides_begin(&f, 600, 250, EYE_Z, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z); // over the street
	CHECK_EQ(FLOOR_HIDES_NOTHING, floor_hides_below(&f, 900, 250));
	floor_hides_begin(&f, 250, 250, ROOF_Z - 10, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z); // below it
	CHECK_EQ(FLOOR_HIDES_NOTHING, floor_hides_below(&f, 900, 250));
	floor_hides_none(&f);
	CHECK_EQ(FLOOR_HIDES_NOTHING, floor_hides_below(&f, 900, 250));
}

static void at_the_edge_hides_nothing_that_way(void)
{
	floor_hides f;

	// Stood 5 from the east edge: looking east down into the street, nothing's hidden that way...
	floor_hides_begin(&f, 495, 250, EYE_Z, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
	CHECK_EQ(FLOOR_HIDES_NOTHING, floor_hides_below(&f, 700, 250));
	// ...but west, across the roof, plenty is
	CHECK(floor_hides_below(&f, -200, 250) > ROOF_Z - 100);
}

static void too_far_to_say_hides_nothing(void)
{
	floor_hides f;

	floor_hides_begin(&f, 250, 250, EYE_Z, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
	CHECK_EQ(FLOOR_HIDES_NOTHING, floor_hides_below(&f, 250 + 20000, 250));
}

static void a_box_is_hidden_only_when_all_of_it_is(void)
{
	floor_hides f;

	floor_hides_begin(&f, 40, 250, EYE_Z, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
	// A lower tower across the street east, its top 200 below the roof: all of it behind the roof
	CHECK(floor_hides_box(&f, 700, 1200, 0, 500, ROOF_Z - 200));
	// As high as the roof: its top shows
	CHECK(!floor_hides_box(&f, 700, 1200, 0, 500, ROOF_Z));
	// Off past the roof's corner (north-east) and only a little lower: its near corner shows
	CHECK(!floor_hides_box(&f, 700, 1200, 700, 1200, ROOF_Z - 20));
}

// A side's foot line at a point `along` of the way (0..256) from its `from` end to its `to` end
static int line_at(int from, int to, int along)
{
	return from + (((to - from) * along) >> 8);
}

/*
 * The user's wedge (2026-11-15, 0650), on this roof: stood 293 from the west edge, the eye 63 up. A
 * tower's side runs west from the roof's north-west corner, along the roof's north edge, standing on
 * the next roof west, 50 lower. Where the floor hides it: up to 992 at the corner end, but only up
 * to ~885 at the far end, below its foot (950). The higher of the two at each end, joined, ran from
 * 950 to 992: halfway along it was at 971, where the side shows from 950 up. A wedge cut out of it
 */
static void a_side_on_a_lower_roof_keeps_its_foot(void)
{
	floor_hides f;
	const int ez = ROOF_Z + 63, foot = ROOF_Z - 50;
	int from = foot, to = foot, hide_from, hide_to;

	floor_hides_begin(&f, 293, 250, ez, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
	hide_from = floor_hides_below(&f, -500, ROOF_MAX); // the far end
	hide_to = floor_hides_below(&f, 0, ROOF_MAX);	  // the corner end
	CHECK(hide_from < foot);						  // below its foot there...
	CHECK(hide_to > foot);							  // ...above it here: the line crosses it
	// The old way: halfway, it cut off what shows
	{
		const int old_from = hide_from > foot ? hide_from : foot, old_to = hide_to > foot ? hide_to : foot;
		const int old_middle = line_at(old_from, old_to, 128);

		CHECK(old_middle > foot + 10);
		CHECK(!truly_hidden(293, 250, ez, -250, ROOF_MAX, old_middle - 5)); // seen, and it was cut
	}
	// Now: the side keeps its foot, all along
	floor_hides_line(foot, &from, &to, hide_from, hide_to);
	CHECK_EQ(foot, from);
	CHECK_EQ(foot, to);
}

// A side deep below, beside the roof (the street's): the floor's line is taken, both ends raised
static void a_side_deep_below_takes_the_line(void)
{
	floor_hides f;
	const int foot = ROOF_Z - 3000;
	int from = foot, to = foot;

	floor_hides_begin(&f, 250, 250, EYE_Z, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
	floor_hides_line(foot, &from, &to, floor_hides_below(&f, 0, 700), floor_hides_below(&f, 500, 700));
	CHECK(from > ROOF_Z - 100);
	CHECK(to > ROOF_Z - 100);
	// A lower line offered after it doesn't replace it
	floor_hides_line(foot, &from, &to, ROOF_Z - 500, ROOF_Z - 500);
	CHECK(from > ROOF_Z - 100);
}

/*
 * Whole sides, not points: random sides near the roof, standing on random feet, their foot line
 * worked out as grey_side_add does. Along each, every height between its own foot and the line is
 * really hidden: the line never cuts off any of a side that shows
 */
static void a_sides_foot_never_cuts_what_shows(void)
{
	floor_hides f;
	int checked = 0;

	for (int n = 0; n < 3000; ++n)
	{
		const int ex = random_in(ROOF_MIN + 1, ROOF_MAX - 1), ey = random_in(ROOF_MIN + 1, ROOF_MAX - 1);
		const int ez = ROOF_Z + random_in(20, 80), foot = ROOF_Z - random_in(1, 300);
		// A side along x at y = line, from x = a to b: north or south of the roof, or across it
		const int line = random_in(0, 1) ? random_in(ROOF_MAX, ROOF_MAX + 1000) : random_in(ROOF_MIN - 1000, ROOF_MIN);
		const int a = random_in(-1500, 1500), b = a + random_in(50, 1500);
		int from = foot, to = foot;

		floor_hides_begin(&f, ex, ey, ez, ROOF_MIN, ROOF_MAX, ROOF_MIN, ROOF_MAX, ROOF_Z);
		floor_hides_line(foot, &from, &to, floor_hides_below(&f, a, line), floor_hides_below(&f, b, line));
		for (int along = 0; along <= 256; along += 16)
		{
			const int x = a + (((b - a) * along) >> 8), top = line_at(from, to, along);

			for (int z = foot; z < top; z += 3)
			{
				if (!truly_hidden(ex, ey, ez, x, line, z))
				{
					CHECK(0); // the side's foot line cuts off some of it that shows
					return;
				}
				++checked;
			}
		}
	}
	CHECK(checked > 10000);
}

void run_floor_hides_tests(void)
{
	RUN(a_side_on_a_lower_roof_keeps_its_foot);
	RUN(a_side_deep_below_takes_the_line);
	RUN(a_sides_foot_never_cuts_what_shows);
	RUN(never_hides_what_shows);
	RUN(hides_nearly_all_it_could);
	RUN(straight_below_the_floor_is_hidden_below_it);
	RUN(off_the_floor_hides_nothing);
	RUN(at_the_edge_hides_nothing_that_way);
	RUN(too_far_to_say_hides_nothing);
	RUN(a_box_is_hidden_only_when_all_of_it_is);
}
