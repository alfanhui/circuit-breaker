#include "test.h"
#include "../src/game/walker.h"

static walker_controls pad(int turn, int move, int strafe)
{
	walker_controls c = {0, 0, 0, 0, 0};

	c.turn = (signed char)turn;
	c.move = (signed char)move;
	c.strafe = (signed char)strafe;
	return c;
}

static void run(walker *w, walker_controls c, const walker_area *area, int frames)
{
	for (int i = 0; i < frames; ++i)
		walker_update(w, c, area);
}

// The table's sine against the right answers, every 90 degrees and in between
static void the_sine_is_right(void)
{
	CHECK_EQ(0, walker_sin(0));
	CHECK_EQ(1024, walker_sin(0x4000));
	CHECK_EQ(0, walker_sin(0x8000));
	CHECK_EQ(-1024, walker_sin(0xC000));
	CHECK_EQ(1024, walker_cos(0));
	CHECK_EQ(-1024, walker_cos(0x8000));
	CHECK(walker_sin(0x1555) >= 511 && walker_sin(0x1555) <= 512); // 30 degrees (to an ANGLE: 511.97),
	CHECK(walker_sin(-0x1555) >= -512 && walker_sin(-0x1555) <= -511); // between two of the table's steps
	CHECK_EQ(724, walker_sin(0x2000)); // 45
	CHECK_EQ(724, walker_sin(0x6000)); // 135: back down the table
	// Every angle within 2 of 1024 sin, and sin^2 + cos^2 close to 1024^2
	for (int a = 0; a < 0x10000; a += 37)
	{
		const int s = walker_sin(a), c = walker_cos(a);
		const int sum = s * s + c * c;

		CHECK(sum > 1020 * 1020 && sum < 1028 * 1028);
	}
}

// Standing still, you stay where you are
static void nothing_held_stays_put(void)
{
	walker w;
	world_point at = {100, -200};

	walker_init(&w, at, 0);
	run(&w, pad(0, 0, 0), 0, 100);
	CHECK_EQ(100, walker_position(&w).x);
	CHECK_EQ(-200, walker_position(&w).y);
	CHECK_EQ(0, walker_bob(&w));
}

// Facing north, walking goes -y; facing east (a quarter turn right), -x. Strafing right while
// facing north goes east too
static void walking_goes_where_you_face(void)
{
	walker w;
	world_point at = {0, 0};

	walker_init(&w, at, 0);
	run(&w, pad(0, 1, 0), 0, 60);
	CHECK(walker_position(&w).y < -100);
	CHECK_EQ(0, walker_position(&w).x);

	walker_init(&w, at, 0x4000);
	run(&w, pad(0, 1, 0), 0, 60);
	CHECK(walker_position(&w).x < -100);
	CHECK_EQ(0, walker_position(&w).y);

	walker_init(&w, at, 0);
	run(&w, pad(0, 0, 1), 0, 60);
	CHECK(walker_position(&w).x < -100);
	CHECK_EQ(0, walker_position(&w).y);

	walker_init(&w, at, 0);
	run(&w, pad(0, -1, 0), 0, 60);
	CHECK(walker_position(&w).y > 50); // backing off: south
}

// Speeds ramp: up to walking in 8 frames, down to nothing in 5. Held on, walking builds to a run
// (running_builds_from_walking). Backing off is slower than walking
static void speeds_ramp_up_and_down(void)
{
	walker w;
	world_point at = {0, 0};

	walker_init(&w, at, 0);
	run(&w, pad(0, 1, 0), 0, 1);
	CHECK_EQ(WALKER_SPEED_UP, w.forward);
	run(&w, pad(0, 1, 0), 0, 7);
	CHECK_EQ(WALKER_FORWARD_SPEED, w.forward);
	run(&w, pad(0, 1, 0), 0, 100);
	CHECK_EQ(WALKER_RUN_SPEED, w.forward); // and no faster
	// Stopping from a run: twice as far to come down, 10 frames
	run(&w, pad(0, 0, 0), 0, 9);
	CHECK(w.forward > 0);
	run(&w, pad(0, 0, 0), 0, 1);
	CHECK_EQ(0, w.forward);

	// Turning about: slowed to a stop first (never past it in one frame), then back the other way
	run(&w, pad(0, 1, 0), 0, 20);
	run(&w, pad(0, -1, 0), 0, 4);
	CHECK(w.forward >= 0);
	run(&w, pad(0, -1, 0), 0, 30);
	CHECK_EQ(-WALKER_BACK_SPEED, w.forward);
	CHECK(-w.forward < WALKER_FORWARD_SPEED); // against the speed reached: MSVC won't check constants
}

// Strafing while you walk or run costs nothing going forwards (play-test 2026-11-09)
static void strafing_keeps_your_speed(void)
{
	walker straight, diagonal;
	world_point at = {0, 0};
	int ds, dd;

	walker_init(&straight, at, 0);
	walker_init(&diagonal, at, 0);
	run(&straight, pad(0, 1, 0), 0, 100);
	run(&diagonal, pad(0, 1, 1), 0, 100);
	ds = -walker_position(&straight).y;
	{
		const int x = walker_position(&diagonal).x, y = walker_position(&diagonal).y;

		dd = x * x + y * y; // compared squared: no square root
	}
	// Forward as far as walking straight (the same run), and sideways on top: further, never less
	CHECK(-walker_position(&diagonal).y == ds);
	CHECK(dd > ds * ds);
}

// A tap turns a little; held, it turns quicker and quicker up to the most; let go, it stops dead
static void turning_speeds_up_and_stops_dead(void)
{
	walker w;
	world_point at = {0, 0};

	walker_init(&w, at, 0);
	run(&w, pad(1, 0, 0), 0, 1);
	CHECK_EQ(WALKER_TURN_FIRST, w.yaw);
	run(&w, pad(1, 0, 0), 0, 1);
	CHECK_EQ(WALKER_TURN_FIRST * 2 + WALKER_TURN_SPEED_UP, w.yaw);
	run(&w, pad(1, 0, 0), 0, 100);
	CHECK_EQ(WALKER_TURN_MAX, w.turn_speed);
	{
		const int yaw = w.yaw;

		run(&w, pad(0, 0, 0), 0, 1);
		CHECK_EQ(yaw, w.yaw); // not a fraction more
	}
	// Left from nothing: the first small step the other way, wrapping round past north
	walker_init(&w, at, 0);
	run(&w, pad(-1, 0, 0), 0, 1);
	CHECK_EQ(0x10000 - WALKER_TURN_FIRST, w.yaw);
	// Right then straight to left: no carrying on right
	run(&w, pad(1, 0, 0), 0, 30);
	run(&w, pad(-1, 0, 0), 0, 1);
	CHECK_EQ(-WALKER_TURN_FIRST, w.turn_speed);
}

// A quarter turn, held from standing, takes about half a second
static void a_quarter_turn_takes_half_a_second(void)
{
	walker w;
	world_point at = {0, 0};
	int frames = 0;

	walker_init(&w, at, 0);
	while (w.yaw < 0x4000 && frames < 200)
	{
		walker_update(&w, pad(1, 0, 0), 0);
		frames++;
	}
	CHECK(frames >= 36 && frames <= 48); // about 0.7 s
}

// A room: its floor (height 0, x -500..500, y -1000..1000) and four walls round it, roofs far too
// tall to step onto
static void room(walker_area *area)
{
	walker_area_clear(area);
	CHECK(walker_area_add(area, 500, 1000, -500, -1000)); // either order
	CHECK(walker_area_add_roof(area, -600, -1100, 600, -1000, 2000)); // north
	CHECK(walker_area_add_roof(area, -600, 1000, 600, 1100, 2000));	  // south
	CHECK(walker_area_add_roof(area, 500, -1100, 600, 1100, 2000));	  // west (+x)
	CHECK(walker_area_add_roof(area, -600, -1100, -500, 1100, 2000)); // east (-x)
}

// Walls keep you WALKER_RADIUS off them; walking into one at an angle slides along it
static void walls_stop_you_and_you_slide(void)
{
	walker_area area;
	walker w;
	world_point at = {0, 0};

	room(&area);
	CHECK(walker_area_holds(&area, 0, 0));
	CHECK(walker_area_holds(&area, 500, 0)); // out to its edge
	CHECK(!walker_area_holds(&area, 0, -2000));

	// Straight at the north wall, for a long time: stopped short of it by the radius (within the
	// last frame's step: the step that would have crossed it isn't taken)
	walker_init(&w, at, 0);
	run(&w, pad(0, 1, 0), &area, 600);
	CHECK(walker_position(&w).y >= -1000 + WALKER_RADIUS && walker_position(&w).y <= -1000 + WALKER_RADIUS + 3);
	CHECK_EQ(0, walker_position(&w).x);
	CHECK_EQ(0, walker_height(&w));

	// At the north-east corner slanting (an eighth turn right): it meets the east wall first, but
	// slides on along it, north, into the corner
	walker_init(&w, at, 0x2000);
	run(&w, pad(0, 1, 0), &area, 600);
	CHECK(walker_position(&w).y >= -1000 + WALKER_RADIUS && walker_position(&w).y <= -1000 + WALKER_RADIUS + 3);
	CHECK(walker_position(&w).x >= -500 + WALKER_RADIUS && walker_position(&w).x <= -500 + WALKER_RADIUS + 3);

	// Room for WALKER_AREA_RECTS_MAX, no more
	for (int i = area.count; i < WALKER_AREA_RECTS_MAX; ++i)
		CHECK(walker_area_add(&area, 0, 0, 1000, 1000));
	CHECK(!walker_area_add(&area, 0, 0, 1000, 1000));
}

// Left nearer a wall than WALKER_RADIUS (a landing can do it: the probe found 29 and 34 off one),
// you can walk away from it and along it, but no further into it. Before, every step was into the
// wall, walking away too, and you were stuck (play-test 2026-11-08: the corner of two grey walls)
static void nearer_a_wall_than_its_margin_you_walk_out(void)
{
	walker_area area;
	walker w;
	world_point at = {0, -1000 + 30}; // 30 off the north wall's face (y = -1000)

	room(&area);
	// Away (south, +y: facing south is half a turn)
	walker_init(&w, at, 0x8000);
	run(&w, pad(0, 1, 0), &area, 30);
	CHECK(walker_position(&w).y > -1000 + WALKER_RADIUS);
	// Along it (east, -x: a quarter turn right): it slides along, the same 30 off
	walker_init(&w, at, 0x4000);
	run(&w, pad(0, 1, 0), &area, 30);
	CHECK(walker_position(&w).x < -50);
	CHECK_EQ(-1000 + 30, walker_position(&w).y);
	// Into it (north): no nearer
	walker_init(&w, at, 0);
	run(&w, pad(0, 1, 0), &area, 30);
	CHECK_EQ(-1000 + 30, walker_position(&w).y);
	// And in the corner (30 off the north wall and 30 off the east one at x = -500), out on the diagonal
	at.x = -500 + 30;
	walker_init(&w, at, 0xA000); // south-west: away from both
	run(&w, pad(0, 1, 0), &area, 30);
	CHECK(walker_position(&w).y > -1000 + WALKER_RADIUS);
	CHECK(walker_position(&w).x > -500 + WALKER_RADIUS);
}

// The face buttons: the left column left, the right right, the top row up, the bottom down; two
// in a column level out
// Walking, the eye bobs once a step, deeper the quicker you go: 3 walking, 6 at a run (and the
// steps come quicker). It dies away as you stop
static void the_eye_bobs_walking(void)
{
	walker w;
	world_point at = {0, 0};
	int deepest = 0, steps = 0, was_down = 0;

	walker_init(&w, at, 0);
	run(&w, pad(0, 1, 0), 0, 10);
	for (int i = 0; i < 120; ++i) // 2 s at full speed
	{
		int bob;

		walker_update(&w, pad(0, 1, 0), 0);
		bob = walker_bob(&w);
		deepest = bob > deepest ? bob : deepest;
		if (bob >= 2 && !was_down)
			steps++;
		was_down = bob >= 2 ? 1 : bob == 0 ? 0 : was_down;
	}
	CHECK_EQ(6, deepest); // held 2 s: a run
	CHECK(steps >= 6 && steps <= 10); // about four a second
	run(&w, pad(0, 0, 0), 0, 10);
	CHECK_EQ(0, walker_bob(&w));
}

/*
 * Roofs for the tests of heights: two side by side, touching along x = 0, the higher (`high`) on
 * the +x side, the lower (`low`) on the -x. Facing east (a quarter turn right) is towards -x:
 * off the high roof's lip, onto the low one
 */
static void two_roofs(walker_area *area, int high, int low)
{
	walker_area_clear(area);
	walker_area_add_roof(area, 0, -1000, 1000, 1000, high);
	walker_area_add_roof(area, -1000, -1000, 0, 1000, low);
}

// Walks east until stopped at the high roof's lip (its edge, x = 0, within the last frame's step),
// its first frame pushing into it. Returns the frames it took (0: never)
static int walk_to_the_lip(walker *w, const walker_area *area)
{
	for (int i = 1; i < 300; ++i)
	{
		walker_update(w, pad(0, 1, 0), area);
		if (w->push > 0)
			return i;
	}
	return 0;
}

// The height at a point is the highest roof there (where two touch, the higher); off them,
// there's none
static void roofs_have_heights(void)
{
	walker_area area;

	two_roofs(&area, 300, 200);
	walker_area_add_roof(&area, 200, -100, 400, 100, 340); // a step up, on the high roof
	CHECK_EQ(300, walker_area_height(&area, 600, 0));
	CHECK_EQ(200, walker_area_height(&area, -500, 0));
	CHECK_EQ(340, walker_area_height(&area, 300, 0));
	CHECK_EQ(300, walker_area_height(&area, 0, 0));				   // the edge they share
	CHECK_EQ(WALKER_NO_FLOOR, walker_area_height(&area, 0, 5000)); // off them all
}

// A kerb (WALKER_STEP_UP) you walk up onto; any higher is a wall: your foot's against a low one,
// your body against a tall one
static void a_kerb_steps_up_a_wall_stops_you(void)
{
	walker w;
	walker_area area;
	world_point at = {260, 0}; // walking up to it: at a run, a step's more than the 4 checked below

	walker_area_clear(&area);
	walker_area_add_roof(&area, -1000, -1000, 1000, 1000, 0);
	walker_area_add_roof(&area, -1000, -1000, 200, 1000, WALKER_STEP_UP); // west of x = 200
	walker_init_at(&w, at, 0, 0x4000); // facing east, -x
	// Up it as your foot reaches it (WALKER_FOOT before your middle does)
	while (walker_position(&w).x > 200 + WALKER_FOOT + 4)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK_EQ(0, walker_height(&w));
	run(&w, pad(0, 1, 0), &area, 2);
	CHECK_EQ(WALKER_STEP_UP, walker_height(&w));
	CHECK(walker_position(&w).x > 200);
	run(&w, pad(0, 1, 0), &area, 300);
	CHECK_EQ(WALKER_STEP_UP, walker_height(&w));
	CHECK(walker_position(&w).x < 0); // and on

	walker_area_clear(&area);
	walker_area_add_roof(&area, -1000, -1000, 1000, 1000, 0);
	walker_area_add_roof(&area, -1000, -1000, 200, 1000, WALKER_STEP_UP + 1);
	at.x = 200 + WALKER_FOOT + 30; // walking up to it: from 30 off, too near to get up to
								   // WALKER_MOVE_SPEED (running, it's a wall you vault: a_full_run_vaults_a_low_wall)
	walker_init_at(&w, at, 0, 0x4000);
	run(&w, pad(0, 1, 0), &area, 300);
	CHECK_EQ(0, walker_height(&w));
	// Stopped with your foot against it (WALKER_FOOT from its edge, x = 200): it's lower than your
	// waist, so it's your foot that meets it
	CHECK(walker_position(&w).x >= 200 + WALKER_FOOT && walker_position(&w).x <= 200 + WALKER_FOOT + 3);
	CHECK(!w.falling);

	// Taller than your waist: your body meets it, WALKER_RADIUS off, as any wall
	walker_area_clear(&area);
	walker_area_add_roof(&area, -1000, -1000, 1000, 1000, 0);
	walker_area_add_roof(&area, -1000, -1000, 200, 1000, WALKER_WAIST + 1);
	at.x = 200 + WALKER_RADIUS + 30;
	walker_init_at(&w, at, 0, 0x4000);
	run(&w, pad(0, 1, 0), &area, 300);
	CHECK_EQ(0, walker_height(&w));
	CHECK(walker_position(&w).x >= 200 + WALKER_RADIUS && walker_position(&w).x <= 200 + WALKER_RADIUS + 3);
}

// Walking into a lower roof's lip stops you there. Push into it long enough and you step off,
// fall, and land on the roof below. Let go before then, and the push starts again
static void a_lip_stops_you_until_you_push(void)
{
	walker w;
	walker_area area;
	world_point at = {30, 0}; // near enough to reach it walking, not running (that jumps it)
	int frames;

	two_roofs(&area, 300, 200);
	walker_init_at(&w, at, 300, 0x4000);
	CHECK(walk_to_the_lip(&w, &area) > 0);
	run(&w, pad(0, 1, 0), &area, WALKER_DROP_PUSH_FRAMES - 3); // pushing, but not long enough
	CHECK(!w.falling);
	CHECK_EQ(300, walker_height(&w));
	CHECK(walker_position(&w).x >= 0 && walker_position(&w).x <= 3); // right at the edge
	run(&w, pad(0, 0, 0), &area, 10); // let go
	CHECK_EQ(0, w.push);
	// And again: from the start. The first frame's push (walk_to_the_lip), and all but the last
	CHECK(walk_to_the_lip(&w, &area) > 0);
	run(&w, pad(0, 1, 0), &area, WALKER_DROP_PUSH_FRAMES - 2);
	CHECK(!w.falling);
	run(&w, pad(0, 1, 0), &area, 1); // long enough
	CHECK(w.falling);
	for (frames = 0; w.falling && frames < 200; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(!w.falling);
	CHECK_EQ(200, walker_height(&w));
	CHECK(walker_position(&w).x < 0);				// on the low roof
	CHECK(walker_dip(&w) > 0);						// the eye dips
	CHECK_EQ(0, w.stagger);							// a clean drop
	CHECK(frames >= 24 && frames <= 34);			// 100 down: 28 frames
}

// A lip with nothing below (a street: a fall to nothing) holds however long you push
static void a_lip_over_nothing_never_lets_go(void)
{
	walker w;
	walker_area area;
	world_point at = {300, 0};

	walker_area_clear(&area);
	walker_area_add_roof(&area, 0, -1000, 1000, 1000, 300);
	walker_init_at(&w, at, 300, 0x4000);
	CHECK(walk_to_the_lip(&w, &area) > 0);
	run(&w, pad(0, 1, 0), &area, 600);
	CHECK(!w.falling);
	CHECK_EQ(300, walker_height(&w));
	CHECK(walker_position(&w).x >= 0 && walker_position(&w).x <= 3); // right at the edge
}

// A drop further than WALKER_CLEAN_DROP staggers you: a creep at most for WALKER_STAGGER_FRAMES,
// then walking as before. The fall takes as long as gravity says: 700 down, ~82 frames (the last
// half at WALKER_FALL_MAX)
static void a_long_drop_staggers_you(void)
{
	walker w;
	walker_area area;
	world_point at = {300, 0};
	int frames, slowest = 0;

	two_roofs(&area, 1000, 300);
	walker_init_at(&w, at, 1000, 0x4000);
	CHECK(walk_to_the_lip(&w, &area) > 0);
	run(&w, pad(0, 1, 0), &area, WALKER_DROP_PUSH_FRAMES);
	CHECK(w.falling);
	for (frames = 0; w.falling && frames < 300; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(frames >= 75 && frames <= 90);
	CHECK_EQ(300, walker_height(&w));
	CHECK(w.stagger > 0);
	CHECK(walker_dip(&w) >= 10);
	// Turned round to walk back into the low roof (west, +x is behind: face -x... east, away from
	// the high roof's wall): held to a creep all through the stagger
	w.yaw = 0x4000;
	run(&w, pad(0, 1, 0), &area, 10); // slowing down to it
	for (int i = 0; i < WALKER_STAGGER_FRAMES - 12; ++i)
	{
		walker_update(&w, pad(0, 1, 0), &area);
		slowest = w.forward > slowest ? w.forward : slowest;
	}
	CHECK(slowest <= WALKER_CREEP_SPEED);
	run(&w, pad(0, 1, 0), &area, 20); // over: up to walking speed again (and on, to a run)
	CHECK_EQ(0, w.stagger);
	CHECK(w.forward >= WALKER_FORWARD_SPEED);
}

// A step up: the feet are up it at once, the eye behind them by the step, catching up a quarter of
// the way a frame (within 10 frames). Down a step, the eye's above for a moment
static void a_step_up_eases_the_eye(void)
{
	walker w;
	walker_area area;
	world_point at = {300, 0};
	int frames;

	walker_area_clear(&area);
	walker_area_add_roof(&area, -1000, -1000, 1000, 1000, 0);
	walker_area_add_roof(&area, -1000, -1000, 200, 1000, WALKER_STEP_UP); // west of x = 200
	walker_init_at(&w, at, 0, 0x4000);
	while (walker_height(&w) == 0)
	{
		CHECK_EQ(0, walker_step_lag(&w));
		walker_update(&w, pad(0, 1, 0), &area);
	}
	CHECK_EQ(WALKER_STEP_UP, walker_step_lag(&w)); // the eye's where it was
	walker_update(&w, pad(0, 1, 0), &area);
	CHECK_EQ(WALKER_STEP_UP * 3 / 4, walker_step_lag(&w)); // a quarter of the way up
	for (frames = 1; walker_step_lag(&w) > 0 && frames < 30; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(frames <= 10);
	CHECK_EQ(0, walker_step_lag(&w));

	// Back down it: facing west (+x), from the step
	w.yaw = 0xC000;
	while (walker_height(&w) == WALKER_STEP_UP)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK_EQ(-WALKER_STEP_UP, walker_step_lag(&w));
	run(&w, pad(0, 1, 0), &area, 12);
	CHECK_EQ(0, walker_step_lag(&w));
}

// Holding forward straight on: walking speed in 8 frames, then on up to a run, 39 frames more.
// Strafing, or turning hard, drops you back. A tap of a turn doesn't
static void running_builds_from_walking(void)
{
	walker w;
	world_point at = {0, 0};

	walker_init(&w, at, 0);
	run(&w, pad(0, 1, 0), 0, 8);
	CHECK_EQ(WALKER_FORWARD_SPEED, w.forward);
	run(&w, pad(0, 1, 0), 0, 7);
	CHECK(w.forward < WALKER_MOVE_SPEED);
	run(&w, pad(0, 1, 0), 0, 1);
	CHECK(w.forward >= WALKER_MOVE_SPEED); // 16 frames: fast enough for the moves
	run(&w, pad(0, 1, 0), 0, 7);
	CHECK(w.forward < WALKER_RUN_SPEED);
	run(&w, pad(0, 1, 0), 0, 1);
	CHECK_EQ(WALKER_RUN_SPEED, w.forward); // 24 frames: 0.4 s

	// Turning, even hard, to line up: still running (play-test 2026-11-09)
	run(&w, pad(1, 1, 0), 0, 30);
	CHECK_EQ(WALKER_RUN_SPEED, w.forward);
	// Strafing too: still running, forward speed and all, and moving sideways as well
	{
		const world_point before = walker_position(&w);

		run(&w, pad(0, 1, 1), 0, 20);
		CHECK_EQ(WALKER_RUN_SPEED, w.forward);
		CHECK_EQ(WALKER_STRAFE_SPEED, w.side);
		CHECK(walker_position(&w).x != before.x && walker_position(&w).y != before.y);
	}
	// Backing off: down to a walk, then backwards
	run(&w, pad(0, -1, 0), 0, 30);
	CHECK(w.forward < 0);
}

// Runs east (-x) from `from` until in the air or at a lip, then on until landed or `frames` are
// up. Returns the highest the feet got
static int run_at_the_lip(walker *w, const walker_area *area, int frames)
{
	int highest = walker_height(w);

	for (int i = 0; i < frames; ++i)
	{
		walker_update(w, pad(0, 1, 0), area);
		highest = walker_height(w) > highest ? walker_height(w) : highest;
		if (i > 0 && !w->falling && highest > walker_height(w) + 20)
			break; // jumped and landed
	}
	return highest;
}

// At a full run, a lip with a clean landing below: you jump off it (up first), fly on at your
// speed, land and run on. Not staggered: it's a clean drop
static void a_full_run_jumps_a_lip(void)
{
	walker w;
	walker_area area;
	world_point at = {600, 0};
	int highest, frames;

	two_roofs(&area, 300, 220);
	walker_init_at(&w, at, 300, 0x4000);
	for (frames = 0; !w.falling && frames < 300; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(w.falling);
	CHECK_EQ(WALKER_RUN_SPEED, w.forward);
	CHECK(walker_position(&w).x >= 0 && walker_position(&w).x <= 6); // from the lip, not before it
	highest = run_at_the_lip(&w, &area, 100);
	CHECK(highest >= 300 + 40); // the hop: 45 up
	CHECK(!w.falling);
	CHECK_EQ(220, walker_height(&w));
	CHECK_EQ(0, w.stagger);
	CHECK(walker_position(&w).x < -100); // carried on out, not dropped at the foot of the lip
	CHECK(walker_dip(&w) > 0);
	run(&w, pad(0, 1, 0), &area, 1);
	CHECK(w.forward >= WALKER_RUN_SPEED - WALKER_SLOW_DOWN); // and you run on
}

// At a full run, a lip with a long drop below: no jump. You stop at it, the run lost, as walking
// (pushing on would step you off, as walking)
static void a_full_run_stops_at_a_long_drop(void)
{
	walker w;
	walker_area area;
	world_point at = {600, 0};

	two_roofs(&area, 300, 0);
	walker_init_at(&w, at, 300, 0x4000);
	CHECK(walk_to_the_lip(&w, &area) > 0);
	run(&w, pad(0, 1, 0), &area, WALKER_DROP_PUSH_FRAMES - 3);
	CHECK(!w.falling);
	CHECK_EQ(300, walker_height(&w));
	CHECK(walker_position(&w).x >= 0 && walker_position(&w).x <= 6);
	CHECK(w.forward <= WALKER_FORWARD_SPEED + WALKER_RUN_SPEED_UP);
}

// Two roofs at the same height with a gap between, nothing below: a run jumps a gap of 200 (4.6
// m: the city's streets), not one of 300. The one too far holds you at the lip, however long you
// push
static void a_full_run_jumps_a_gap_in_reach(void)
{
	walker w;
	walker_area area;
	world_point at = {1000, 0};

	walker_area_clear(&area);
	walker_area_add_roof(&area, 0, -1000, 2000, 1000, 300);
	walker_area_add_roof(&area, -1000, -1000, -200, 1000, 300);
	walker_init_at(&w, at, 300, 0x4000);
	CHECK(run_at_the_lip(&w, &area, 400) >= 300 + 40); // up near your waist at the top
	CHECK(!w.falling);
	CHECK_EQ(300, walker_height(&w));
	CHECK(walker_position(&w).x < -200); // over

	walker_area_clear(&area);
	walker_area_add_roof(&area, 0, -1000, 2000, 1000, 300);
	walker_area_add_roof(&area, -1000, -1000, -300, 1000, 300);
	walker_init_at(&w, at, 300, 0x4000);
	run(&w, pad(0, 1, 0), &area, 600);
	CHECK(!w.falling);
	CHECK_EQ(300, walker_height(&w));
	CHECK(walker_position(&w).x >= 0 && walker_position(&w).x <= 6); // held at the lip
}

// A jump up: across a gap of 200 onto a roof 60 higher than you left. You can't land on it (the
// jump's top is 45 up), but you go into its side with its top in reach, catch the edge and climb
// up (walker.c's catch_ledge): stood on it, at a walk. 160 higher is out of reach: held at the lip
static void a_jump_catches_a_ledge_above(void)
{
	walker w;
	walker_area area;
	world_point at = {1000, 0};
	int frames, caught = 0;

	walker_area_clear(&area);
	walker_area_add_roof(&area, 0, -1000, 2000, 1000, 300);
	walker_area_add_roof(&area, -1000, -1000, -200, 1000, 360);
	walker_init_at(&w, at, 300, 0x4000);
	for (frames = 0; frames < 600 && walker_height(&w) != 360; ++frames)
	{
		walker_update(&w, pad(0, 1, 0), &area);
		caught |= walker_move(&w) == WALKER_MOVE_CLIMB;
	}
	CHECK(!w.falling);
	CHECK_EQ(360, walker_height(&w));
	CHECK(caught);
	CHECK(walker_position(&w).x < -200);
	CHECK(w.forward <= WALKER_FORWARD_SPEED);

	walker_area_clear(&area);
	walker_area_add_roof(&area, 0, -1000, 2000, 1000, 300);
	walker_area_add_roof(&area, -1000, -1000, -200, 1000, 460);
	walker_init_at(&w, at, 300, 0x4000);
	run(&w, pad(0, 1, 0), &area, 600);
	CHECK(!w.falling);
	CHECK_EQ(300, walker_height(&w));
	CHECK(walker_position(&w).x >= 0 && walker_position(&w).x <= 6); // held at the lip
}

// In the air over nothing (a street): not fallen off at first (you feel the fall), then once
// you're WALKER_LOST_DROP down, you have. Over a roof you'll land on, never
static void falling_into_a_street_is_the_end(void)
{
	walker w;
	walker_area area;
	world_point street = {-500, 0}, roof = {500, 0};
	int frames, lost_at = -1;

	walker_area_clear(&area);
	walker_area_add_roof(&area, 0, -1000, 2000, 1000, 0);
	walker_init_at(&w, street, 300, 0);
	w.falling = 1;
	w.fall_from = 300;
	w.take_off = 300;
	for (frames = 0; frames < 200 && lost_at < 0; ++frames)
	{
		walker_update(&w, pad(0, 0, 0), &area);
		if (walker_fell_off(&w, &area))
			lost_at = frames;
	}
	CHECK(lost_at > 30); // a moment's fall first
	CHECK(walker_height(&w) < 300 - WALKER_LOST_DROP);

	walker_init_at(&w, roof, 300, 0);
	w.falling = 1;
	w.fall_from = 300;
	w.take_off = 300;
	for (frames = 0; frames < 200; ++frames)
	{
		walker_update(&w, pad(0, 0, 0), &area);
		CHECK(!walker_fell_off(&w, &area));
	}
	CHECK_EQ(0, walker_height(&w)); // landed (a long drop: a stagger, not the end)
}

// Nothing steers you in the air: turning in a jump, you land where you would have
static void the_air_doesnt_steer(void)
{
	walker straight, turning;
	walker_area area;
	world_point at = {600, 0};

	two_roofs(&area, 300, 220);
	walker_init_at(&straight, at, 300, 0x4000);
	while (!straight.falling)
		walker_update(&straight, pad(0, 1, 0), &area);
	turning = straight;
	while (straight.falling)
		walker_update(&straight, pad(0, 1, 0), &area);
	while (turning.falling)
		walker_update(&turning, pad(1, 1, 0), &area);
	CHECK_EQ(walker_position(&straight).x, walker_position(&turning).x);
	CHECK_EQ(walker_position(&straight).y, walker_position(&turning).y);
	CHECK(turning.yaw != straight.yaw); // it did turn: to look
}

// A low wall (above a kerb, no higher than your waist): walking, it stops you; at a full run you
// vault up onto it, the eye following
static void a_full_run_vaults_a_low_wall(void)
{
	walker w;
	walker_area area;
	world_point at = {1000, 0};
	const int wall = 40;

	walker_area_clear(&area);
	walker_area_add_roof(&area, -1000, -1000, 2000, 1000, 0);
	walker_area_add_roof(&area, -1000, -1000, 200, 1000, wall); // west of x = 200
	walker_init_at(&w, at, 0, 0x4000);
	while (walker_height(&w) == 0 && walker_position(&w).x > 0)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK_EQ(wall, walker_height(&w));
	CHECK_EQ(wall, walker_step_lag(&w)); // the eye's still down there
	CHECK(walker_position(&w).x <= 200); // carried onto its top
	run(&w, pad(0, 1, 0), &area, 20);
	CHECK_EQ(wall, walker_height(&w));
	CHECK_EQ(0, walker_step_lag(&w));
	CHECK_EQ(WALKER_RUN_SPEED, w.forward); // without breaking stride
}

// A floor, and west of x = 200 a box `height` high and `depth` deep (from 200 down to 200 - depth),
// the floor going on beyond it. Running at it from x = 1000, facing east (-x)
static void box_ahead(walker_area *area, int height, int depth)
{
	walker_area_clear(area);
	walker_area_add_roof(area, -2000, -1000, 2000, 1000, 0);
	walker_area_add_roof(area, 200 - depth, -1000, 200, 1000, height);
}

// A ledge above your waist, up to WALKER_CLIMB: walking, a wall; at a full run you climb up onto
// it. Slower than a vault (the eye takes longer to follow), and it costs the run. Higher is a
// wall, run or not
static void a_full_run_climbs_a_ledge(void)
{
	walker w;
	walker_area area;
	world_point at = {1000, 0};
	int frames;

	box_ahead(&area, 90, 1000);
	walker_init_at(&w, at, 0, 0x4000);
	// Walking up to it (from 30 off: too near to get up to WALKER_MOVE_SPEED): stopped at its foot
	{
		walker walking;
		const world_point near = {200 + WALKER_RADIUS + 30, 0};

		walker_init_at(&walking, near, 0, 0x4000);
		for (frames = 0; frames < 400; ++frames)
			walker_update(&walking, pad(0, 1, 0), &area);
		CHECK_EQ(0, walker_height(&walking));
		CHECK(walker_position(&walking).x >= 200);
	}
	// Running: up onto it
	for (frames = 0; frames < 400 && walker_height(&w) == 0; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK_EQ(90, walker_height(&w));
	CHECK(walker_position(&w).x <= 200); // on its top
	CHECK_EQ(90, walker_step_lag(&w));	 // the eye's still down there
	CHECK(w.forward <= WALKER_FORWARD_SPEED); // a climb takes your run
	run(&w, pad(0, 0, 0), &area, 6);
	CHECK(walker_step_lag(&w) > 40); // following at half a step's pace: still well below after 6
	run(&w, pad(0, 0, 0), &area, 40);
	CHECK_EQ(0, walker_step_lag(&w));

	// Just above the climb: a wall
	box_ahead(&area, WALKER_CLIMB + 1, 1000);
	walker_init_at(&w, at, 0, 0x4000);
	run(&w, pad(0, 1, 0), &area, 400);
	CHECK_EQ(0, walker_height(&w));
	CHECK(walker_position(&w).x >= 200 + WALKER_RADIUS && walker_position(&w).x <= 200 + WALKER_RADIUS + 6);
}

// Dropped from one roof to the next one down (60: a clean drop), you can run back and climb up
// again: drops in the city aren't one way
static void a_drop_can_be_climbed_back(void)
{
	walker w;
	walker_area area;
	world_point at = {-800, 0};

	two_roofs(&area, 300, 240); // the high roof's west of x = 0 (+x), the low one east
	walker_init_at(&w, at, 240, 0xC000); // facing west (+x), at the high roof's side
	for (int frames = 0; frames < 400 && walker_height(&w) == 240; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK_EQ(300, walker_height(&w));
	CHECK(walker_position(&w).x >= 0);
}

// Something low and thin (a rail, 30 deep): walking, it stops you. At a full run you vault over
// it: your feet onto its near edge, a small hop, and you land beyond it on the floor, running on.
// A deep one (a crate's top you'd come down on) you vault up onto instead
static void a_full_run_vaults_over_a_rail(void)
{
	walker w;
	walker_area area;
	world_point at = {1000, 0};
	int frames;

	box_ahead(&area, 40, 30);
	walker_init_at(&w, at, 0, 0x4000);
	for (frames = 0; frames < 400 && !w.falling; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(w.falling);
	CHECK_EQ(40, walker_height(&w)); // hands on its top
	CHECK(walker_position(&w).x <= 200 && walker_position(&w).x >= 170);
	CHECK_EQ(WALKER_RUN_SPEED, w.forward);
	for (frames = 0; frames < 100 && w.falling; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(!w.falling);
	CHECK_EQ(0, walker_height(&w)); // down beyond it
	CHECK(walker_position(&w).x < 170);
	CHECK_EQ(0, w.stagger);
	run(&w, pad(0, 1, 0), &area, 1);
	CHECK(w.forward >= WALKER_RUN_SPEED - WALKER_SLOW_DOWN); // running on

	// Walking (from 34 off: too near to get up to WALKER_MOVE_SPEED): a wall at your foot
	{
		const world_point near = {200 + WALKER_FOOT + 34, 0};

		walker_init_at(&w, near, 0, 0x4000);
	}
	run(&w, pad(0, 1, 0), &area, 400);
	CHECK_EQ(0, walker_height(&w));
	CHECK(walker_position(&w).x >= 200);

	// Deep: up onto it, not over
	box_ahead(&area, 40, 600);
	walker_init_at(&w, at, 0, 0x4000);
	for (frames = 0; frames < 400 && walker_height(&w) == 0; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK_EQ(40, walker_height(&w));
	CHECK(!w.falling);
}

// Lands a jump off the high roof's lip from `from` (x, east to x = 0), and returns how far it
// carried you past the lip: from your x as you left the roof to your x on landing
static int jump_length_from(int from)
{
	walker w;
	walker_area area;
	const world_point at = {(short)from, 0};
	int took_off = 0, frames;

	two_roofs(&area, 300, 220);
	walker_init_at(&w, at, 300, 0x4000);
	for (frames = 0; !w.falling && frames < 300; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(w.falling);
	took_off = walker_position(&w).x;
	run_at_the_lip(&w, &area, 100);
	CHECK(!w.falling);
	return took_off - walker_position(&w).x;
}

// A short run-up does: from WALKER_MOVE_SPEED (16 frames from standing, ~45 along) the moves are
// taken, and each is taken at a full run (kick_to_a_run), so a jump's as long as from a long one
static void a_short_run_up_is_enough(void)
{
	walker w;
	walker_area area;
	const world_point rail = {200 + WALKER_FOOT + 60, 0};
	int frames, short_jump, long_jump;

	// A jump off a lip: from 60 back, and from 600
	short_jump = jump_length_from(60);
	long_jump = jump_length_from(600);
	CHECK(short_jump > 100);
	CHECK(short_jump >= long_jump - 6 && short_jump <= long_jump + 6); // a frame's step either way

	// A vault over a rail, from 60 short of where your foot meets it
	box_ahead(&area, 40, 30);
	walker_init_at(&w, rail, 0, 0x4000);
	for (frames = 0; frames < 100 && !w.falling; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(w.falling);
	CHECK_EQ(40, walker_height(&w)); // hands on its top
	CHECK_EQ(WALKER_RUN_SPEED, w.forward); // kicked to a run
	for (frames = 0; frames < 100 && w.falling; ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK_EQ(0, walker_height(&w));
	CHECK(walker_position(&w).x < 170); // over it
}

// A floor, and a bar across it west of x = 200, `depth` deep, from `bottom` up to 140
static void bar_ahead(walker_area *area, int bottom, int depth)
{
	walker_area_clear(area);
	walker_area_add_roof(area, -2000, -1000, 2000, 1000, 0);
	CHECK(walker_area_add_bar(area, 200 - depth, -1000, 200, 1000, bottom, 140));
}

// A bar at your chest (bottom 50: above a slide, below your head): walking, it stops you. At a full
// run you slide under it, low, the pad not steering, and come up beyond it with room to stand.
// Lower than a slide clears, or too deep to come out from under, it stops you, run or not
static void a_full_run_slides_under_a_bar(void)
{
	walker w;
	walker_area area;
	world_point at = {1000, 0};
	int frames, lowest = WALKER_EYE;

	bar_ahead(&area, 50, 60);
	CHECK_EQ(140, walker_area_height(&area, 170, 0)); // its top's a roof

	// Walking (from 30 off: too near to get up to WALKER_MOVE_SPEED): stopped by it, your body off it
	{
		const world_point near = {200 + WALKER_RADIUS + 30, 0};

		walker_init_at(&w, near, 0, 0x4000);
	}
	run(&w, pad(0, 1, 0), &area, 400);
	CHECK(walker_position(&w).x >= 200);

	// Running: a slide
	walker_init_at(&w, at, 0, 0x4000);
	for (frames = 0; frames < 400 && !walker_sliding(&w); ++frames)
		walker_update(&w, pad(0, 1, 0), &area);
	CHECK(walker_sliding(&w));
	CHECK(walker_position(&w).x >= 200);
	for (frames = 0; frames < 100 && walker_sliding(&w); ++frames)
	{
		walker_update(&w, pad(1, 0, 0), &area); // turning and letting go: neither steers it
		lowest = walker_eye(&w) < lowest ? walker_eye(&w) : lowest;
	}
	CHECK(!walker_sliding(&w));
	CHECK_EQ(WALKER_SLIDE_FRAMES, frames);
	CHECK(lowest <= WALKER_EYE_SLIDE + 2);
	CHECK(walker_position(&w).x < 200 - 60 - WALKER_RADIUS); // out the far side, standing room
	CHECK_EQ(0, walker_position(&w).y);						 // straight on
	CHECK_EQ(0x4000, w.yaw);
	CHECK_EQ(0, walker_height(&w));
	CHECK(w.forward <= WALKER_FORWARD_SPEED);
	run(&w, pad(0, 0, 0), &area, 60);
	CHECK_EQ(WALKER_EYE, walker_eye(&w)); // stood back up

	// Too low to slide under (bottom 30): a wall
	bar_ahead(&area, 30, 60);
	walker_init_at(&w, at, 0, 0x4000);
	run(&w, pad(0, 1, 0), &area, 400);
	CHECK(!walker_sliding(&w));
	CHECK(walker_position(&w).x >= 200);

	// Too deep to come out from under (400): no slide
	bar_ahead(&area, 50, 400);
	walker_init_at(&w, at, 0, 0x4000);
	run(&w, pad(0, 1, 0), &area, 400);
	CHECK(!walker_sliding(&w));
	CHECK(walker_position(&w).x >= 200);

	// High enough to walk under (bottom 80, over your head): you just go on
	bar_ahead(&area, 80, 60);
	walker_init_at(&w, at, 0, 0x4000);
	run(&w, pad(0, 1, 0), &area, 400);
	CHECK(walker_position(&w).x < 0);
}


void run_walker_tests(void)
{
	RUN(a_step_up_eases_the_eye);
	RUN(running_builds_from_walking);
	RUN(a_full_run_jumps_a_lip);
	RUN(a_full_run_stops_at_a_long_drop);
	RUN(a_full_run_jumps_a_gap_in_reach);
	RUN(a_jump_catches_a_ledge_above);
	RUN(falling_into_a_street_is_the_end);
	RUN(the_air_doesnt_steer);
	RUN(a_full_run_vaults_a_low_wall);
	RUN(a_full_run_climbs_a_ledge);
	RUN(a_drop_can_be_climbed_back);
	RUN(a_full_run_vaults_over_a_rail);
	RUN(a_short_run_up_is_enough);
	RUN(a_full_run_slides_under_a_bar);
	RUN(roofs_have_heights);
	RUN(a_kerb_steps_up_a_wall_stops_you);
	RUN(a_lip_stops_you_until_you_push);
	RUN(a_lip_over_nothing_never_lets_go);
	RUN(a_long_drop_staggers_you);
	RUN(the_sine_is_right);
	RUN(nothing_held_stays_put);
	RUN(walking_goes_where_you_face);
	RUN(speeds_ramp_up_and_down);
	RUN(strafing_keeps_your_speed);
	RUN(turning_speeds_up_and_stops_dead);
	RUN(a_quarter_turn_takes_half_a_second);
	RUN(walls_stop_you_and_you_slide);
	RUN(nearer_a_wall_than_its_margin_you_walk_out);
	RUN(the_eye_bobs_walking);
}
