#include "test.h"
#include "../src/game/arrival_flight.h"
#include "../src/game/arena.h"

static int distance(int a, int b)
{
	return a > b ? a - b : b - a;
}

// The spectator's camera with the steady point at pan_x (view.c's SPECTATE_*, the message's line
// at y 0): from the start of a message to its end the seat runs from x ~12400 to ~-2600
static intro_shot seat(int pan_x)
{
	intro_shot s = {.ox = pan_x + 700 + 4079, .oy = 7064, .height = 3804, .rx = INTRO_DEG(65), .rz = INTRO_DEG(30),
					.roll = 0, .fov = INTRO_DEG(30), .lead = 0, .follow = 0, .fade_start = 9700, .fade_end = 20000};

	return s;
}

static world_point start_point(void)
{
	world_point p = {0, 0}; // input.c's PLAYER1_START: the arena's centre
	return p;
}

static void starts_in_the_stands(void)
{
	intro_shot from = seat(3000);
	intro_shot s;

	arrival_flight_at(&from, start_point(), 0, &s);
	CHECK_EQ(from.ox, s.ox);
	CHECK_EQ(from.oy, s.oy);
	CHECK_EQ(3804, s.height);
	CHECK_EQ(INTRO_DEG(30), s.rz);
}

// The last frame is the 1st person camera exactly (view.c's FIRST_PERSON_*, on the cycle facing
// north), with its fog: the hand-over can't jump
static void lands_in_the_cycle(void)
{
	intro_shot from = seat(3000);
	intro_shot s;
	world_point start = {-300, 200};

	arrival_flight_at(&from, start, ARRIVAL_FRAMES, &s);
	CHECK_EQ(-300, s.ox);
	CHECK_EQ(200, s.oy);
	CHECK_EQ(35, s.height); // -FIRST_PERSON_Z
	CHECK_EQ(16384, s.rx);	// DEGtoANG(90.0): level
	CHECK_EQ(0, s.rz);		// north
	CHECK_EQ(16384, s.fov); // 90 degrees
	CHECK_EQ(400, s.fade_start);
	CHECK_EQ(3000, s.fade_end);
	CHECK_EQ(180, ARRIVAL_FRAMES); // 3 s
	CHECK(arrival_key_name(1)[0] == 'o'); // "overview"
	CHECK(arrival_key_name(99)[0] == 'c'); // past the end: the last, "cycle"
}

/*
 * From wherever the spectator's seat had got to along the message:
 *   - never below the 1st person eye (the curve mustn't dip into the cycle or the floor), and
 *     never looking above level
 *   - low down (among the walls, ARRIVAL_LOW_HEIGHT), well inside the arena: view.c's wall
 *     clipping then works as in 1st person, which needs every wall far from the camera
 *   - the turn only goes one way, back to north, and nothing turns more than a degree a frame
 */
#define WALL_ROOM 2500
static void every_seat_flies_down_safely(void)
{
	int lowest = 100000;
	int nearest_wall_low = 100000;
	int worst_angle = 0;

	arena_set_pieces(ARENA_PIECES_DEFAULT);
	for (int pan_x = -7300; pan_x <= 7700; pan_x += 500)
	{
		intro_shot from = seat(pan_x);
		intro_shot before;
		intro_shot now;

		arrival_flight_at(&from, start_point(), 0, &before);
		for (int f = 1; f <= ARRIVAL_FRAMES; ++f)
		{
			arrival_flight_at(&from, start_point(), f, &now);
			if (now.height < lowest)
				lowest = now.height;
			CHECK(now.rx >= 0 && now.rx <= INTRO_DEG(90));
			if (now.height < ARRIVAL_LOW_HEIGHT)
			{
				int wall = arena_half() - (distance(now.ox, 0) > distance(now.oy, 0) ? distance(now.ox, 0)
																						: distance(now.oy, 0));

				if (wall < nearest_wall_low)
					nearest_wall_low = wall;
			}
			CHECK(now.rz <= before.rz + 1); // only turning back left
			if (distance(now.rz, before.rz) > worst_angle)
				worst_angle = distance(now.rz, before.rz);
			if (distance(now.rx, before.rx) > worst_angle)
				worst_angle = distance(now.rx, before.rx);
			before = now;
		}
	}
	printf("    lowest %d; nearest wall while low %d; worst turn a frame %d\n", lowest, nearest_wall_low, worst_angle);
	CHECK(lowest >= 34); // 35, less a unit of the curve's rounding
	CHECK(nearest_wall_low >= WALL_ROOM);
	CHECK(worst_angle <= 182);
}

// The walls are flat in the floor as the camera leaves the stands, rise smoothly, and stand full
// height before the camera comes down among them
static void the_walls_rise_before_it_lands(void)
{
	intro_shot from = seat(0);
	intro_shot s;
	int before = 0;

	CHECK_EQ(0, arrival_wall_rise(0));
	CHECK_EQ(0, arrival_wall_rise(ARRIVAL_RISE_FROM));
	for (int f = 1; f <= ARRIVAL_FRAMES; ++f)
	{
		int rise = arrival_wall_rise(f);

		CHECK(rise >= before);
		CHECK(rise - before <= 7); // no jumps: at most ~40 of the wall's 1500 a frame
		before = rise;
		arrival_flight_at(&from, start_point(), f, &s);
		if (s.height < ARRIVAL_LOW_HEIGHT)
			CHECK(rise == ARRIVAL_RISE_FULL || s.height > 1000);
	}
	CHECK_EQ(ARRIVAL_RISE_FULL, arrival_wall_rise(ARRIVAL_FRAMES));
}

void run_arrival_flight_tests(void)
{
	RUN(starts_in_the_stands);
	RUN(lands_in_the_cycle);
	RUN(every_seat_flies_down_safely);
	RUN(the_walls_rise_before_it_lands);
}
