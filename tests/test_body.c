#include "test.h"
#include "../src/game/body.h"
#include "../src/game/frustum.h"
#include "../src/game/walker.h"

/*
 * Your body (game/body.h): one skeleton, posed once a frame. Its own coordinates, x16: x right,
 * y the way you face, z up from the ground under your eye
 */

#define U(units) ((units)*BODY_SUB)
#define STOOD U(WALKER_EYE)

static body_state doing(int mode, int eye, int stride, int speed)
{
	body_state s;

	s.mode = mode;
	s.eye = eye;
	s.stride = stride;
	s.speed = speed;
	return s;
}

static int apart(body_point a, body_point b)
{
	const int dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;

	return frustum_isqrt((unsigned int)(dx * dx + dy * dy + dz * dz));
}

static int near(int a, int b, int within)
{
	const int d = a - b;

	return d <= within && d >= -within;
}

// Every limb's two bones are as long as they should be (within a third of a unit: rounding)
static void check_bones(const body *b)
{
	for (int k = 0; k < 2; ++k)
	{
		const int base = k * BODY_SIDE;

		CHECK(near(apart(b->joint[BODY_SHOULDER + base], b->joint[BODY_ELBOW + base]), U(BODY_UPPER_ARM), 6));
		CHECK(near(apart(b->joint[BODY_ELBOW + base], b->joint[BODY_WRIST + base]), U(BODY_FOREARM), 6));
		CHECK(near(apart(b->joint[BODY_HIP + base], b->joint[BODY_KNEE + base]), U(BODY_THIGH), 6));
		CHECK(near(apart(b->joint[BODY_KNEE + base], b->joint[BODY_ANKLE + base]), U(BODY_SHIN), 6));
	}
	CHECK(near(apart(b->joint[BODY_NECK], b->joint[BODY_PELVIS]), U(BODY_TORSO), 6));
}

// Whether a point's inside what your eyes see looking level: in front, and no more than ~29
// degrees below the eye's height or ~36 to either side (the screen's edges: view.c)
static int in_view(const body *b, int eye, int joint)
{
	const body_point p = b->joint[joint];
	const int below = eye - p.z, aside = p.x < 0 ? -p.x : p.x;

	return p.y > U(8) && below * 16 < p.y * 9 && below > -p.y / 2 && aside * 4 < p.y * 3;
}

// Nothing stretches, whatever you're doing, and through every change from one thing to the next
static void bones_never_stretch(void)
{
	static const int modes[] = {BODY_MOVING, BODY_AIR, BODY_MOVING, BODY_SLIDE, BODY_BRACE, BODY_LIE, BODY_RISE, BODY_MOVING};
	static const int eyes[] = {WALKER_EYE, WALKER_EYE, WALKER_EYE, WALKER_EYE_SLIDE, 25, 25, 40, WALKER_EYE_RIDING};
	body b;

	body_init(&b);
	for (int m = 0; m < 8; ++m)
		for (int frame = 0; frame < 60; ++frame)
		{
			const body_state s = doing(modes[m], U(eyes[m]), frame * 1500, m == 2 ? WALKER_RUN_SPEED : WALKER_FORWARD_SPEED);

			body_update(&b, &s);
			check_bones(&b);
		}
}

// Stood still: the same both sides, arms hanging below the shoulders, feet on the ground under you
static void standing_is_even(void)
{
	const body_state s = doing(BODY_MOVING, STOOD, 0x2345, 0);
	body b;

	body_init(&b);
	body_update(&b, &s);
	CHECK_EQ(-b.joint[BODY_WRIST].x, b.joint[BODY_WRIST_R].x);
	CHECK_EQ(b.joint[BODY_WRIST].z, b.joint[BODY_WRIST_R].z);
	CHECK(b.joint[BODY_WRIST].z < b.joint[BODY_SHOULDER].z - U(20));
	CHECK(b.joint[BODY_WRIST].x < 0); // the left's on the left
	CHECK_EQ(U(2), b.joint[BODY_ANKLE].z);
	CHECK_EQ(0, b.joint[BODY_TOES].z);
	CHECK_EQ(0, b.joint[BODY_ANKLE].y);
	CHECK_EQ(STOOD + U(BODY_EYE_TO_CROWN), b.joint[BODY_CROWN].z);
	CHECK(b.joint[BODY_KNEE].z > b.joint[BODY_ANKLE].z);
	CHECK(b.joint[BODY_KNEE].y >= 0); // knees bend forwards
	// Out of sight, looking level: honest arms are
	CHECK(!in_view(&b, STOOD, BODY_FINGERS));
	CHECK(!in_view(&b, STOOD, BODY_FINGERS_R));
}

// Running: arms against legs, and the hand that's forward comes up into the bottom of your view
static void running_pumps_the_arms_into_view(void)
{
	body b;
	int seen_left = 0, seen_right = 0;

	body_init(&b);
	for (int frame = 0; frame < 200; ++frame)
	{
		// The walker's stride moves on 1.5 x its speed a frame
		const body_state s = doing(BODY_MOVING, STOOD, frame * 2304, WALKER_RUN_SPEED);

		body_update(&b, &s);
		if (frame < 20)
			continue; // into its stride
		// The left foot forward: the left hand's back and the right forward, and the other way
		if (b.joint[BODY_ANKLE].y > U(12))
		{
			CHECK(b.joint[BODY_WRIST].y < b.joint[BODY_WRIST_R].y);
			CHECK(b.joint[BODY_ANKLE_R].y < 0);
		}
		if (b.joint[BODY_ANKLE_R].y > U(12))
			CHECK(b.joint[BODY_WRIST_R].y < b.joint[BODY_WRIST].y);
		CHECK(b.joint[BODY_ANKLE].z >= U(2) - 2 && b.joint[BODY_ANKLE_R].z >= U(2) - 2); // never through the floor
		seen_left |= in_view(&b, STOOD, BODY_FINGERS);
		seen_right |= in_view(&b, STOOD, BODY_FINGERS_R);
		// Never both up at once: they take turns
		CHECK(!(in_view(&b, STOOD, BODY_WRIST) && in_view(&b, STOOD, BODY_WRIST_R)));
	}
	CHECK(seen_left);
	CHECK(seen_right);
}

// Walking's quieter: the arms swing as they hang, and stay out of your view
static void walking_keeps_the_arms_down(void)
{
	body b;
	int forward = -30000, back = 30000;

	body_init(&b);
	for (int frame = 0; frame < 200; ++frame)
	{
		const body_state s = doing(BODY_MOVING, STOOD, frame * 1152, WALKER_FORWARD_SPEED);

		body_update(&b, &s);
		if (frame < 20)
			continue;
		CHECK(!in_view(&b, STOOD, BODY_FINGERS));
		forward = b.joint[BODY_WRIST].y > forward ? b.joint[BODY_WRIST].y : forward;
		back = b.joint[BODY_WRIST].y < back ? b.joint[BODY_WRIST].y : back;
	}
	CHECK(forward - back > U(8)); // but it does swing
}

// Thrown from the cycle: both hands out in front of your face, at once enough to be seen going down
static void the_crash_throws_your_hands_out(void)
{
	const body_state s = doing(BODY_BRACE, U(30), 0, 0);
	body b;

	body_init(&b);
	for (int frame = 0; frame < 12; ++frame)
		body_update(&b, &s);
	CHECK(in_view(&b, U(30), BODY_WRIST));
	CHECK(in_view(&b, U(30), BODY_WRIST_R));
	CHECK(b.joint[BODY_WRIST].x < 0 && b.joint[BODY_WRIST_R].x > 0);
	// Legs out behind, on the floor
	CHECK(b.joint[BODY_ANKLE].y < b.joint[BODY_PELVIS].y);
	CHECK(b.joint[BODY_PELVIS].y < b.joint[BODY_NECK].y);
}

// Lying where you stopped: hands on the floor, then the right one lifts in front of your face
static void lying_you_look_at_your_hand(void)
{
	const body_state s = doing(BODY_LIE, U(25), 0, 0);
	body b;

	body_init(&b);
	for (int frame = 0; frame < BODY_LIE_LOOK; ++frame)
		body_update(&b, &s);
	CHECK(b.joint[BODY_WRIST_R].z < U(8));
	CHECK(b.joint[BODY_WRIST].z < U(8));
	for (int frame = 0; frame < 30; ++frame)
		body_update(&b, &s);
	CHECK(in_view(&b, U(25), BODY_WRIST_R));
	CHECK(b.joint[BODY_WRIST].z < U(8)); // the left stays down
}

// Getting up: the hands push on the floor while it's in reach, and hang once you're up
static void getting_up_pushes_off_the_floor(void)
{
	body b;

	body_init(&b);
	for (int frame = 0; frame < 20; ++frame)
	{
		const body_state s = doing(BODY_RISE, U(28), 0, 0);

		body_update(&b, &s);
	}
	CHECK(b.joint[BODY_WRIST].z < U(8) && b.joint[BODY_WRIST_R].z < U(8));
	for (int frame = 0; frame < 30; ++frame)
	{
		const body_state s = doing(BODY_RISE, STOOD, 0, 0);

		body_update(&b, &s);
	}
	CHECK(b.joint[BODY_WRIST].z > U(25));
	CHECK(near(b.joint[BODY_ANKLE].y, 0, U(1))); // feet under you
}

// A slide: legs out ahead of you, where your eyes (low, 26 up) can see them
static void sliding_your_legs_go_ahead(void)
{
	const body_state s = doing(BODY_SLIDE, U(WALKER_EYE_SLIDE), 0, WALKER_RUN_SPEED);
	body b;

	body_init(&b);
	for (int frame = 0; frame < 20; ++frame)
		body_update(&b, &s);
	CHECK(b.joint[BODY_PELVIS].y > b.joint[BODY_NECK].y); // leant back on it
	CHECK(b.joint[BODY_ANKLE].y > U(40));
	CHECK(in_view(&b, U(WALKER_EYE_SLIDE), BODY_ANKLE));
}

// A change of pose eases: the hand never jumps (under 10 units in a frame: a quarter of the way), from standing to a
// jump and back
static void changes_ease(void)
{
	body b;
	body_point before;

	body_init(&b);
	for (int frame = 0; frame < 90; ++frame)
	{
		const body_state s = doing(frame < 30 || frame >= 60 ? BODY_MOVING : BODY_AIR, STOOD, 0, 0);

		before = b.joint[BODY_WRIST];
		body_update(&b, &s);
		if (frame > 0)
			CHECK(apart(before, b.joint[BODY_WRIST]) < U(10));
	}
	// In the air the hands were up, in the lower corners of your view
	{
		const body_state s = doing(BODY_AIR, STOOD, 0, 0);

		for (int frame = 0; frame < 20; ++frame)
			body_update(&b, &s);
		CHECK(in_view(&b, STOOD, BODY_WRIST));
		CHECK(in_view(&b, STOOD, BODY_WRIST_R));
	}
}

/*
 * The moves' hands, with the walker itself: a floor, and a box of any height and depth across your
 * way at x = 200 (as test_walker.c's). You run at it from x = 1000, facing east (-x); each frame
 * the body's told what the walker's doing, as input.c does, and its eye is the camera's: the
 * walker's eye less its lag. Nothing here, or in the body, is told the box's size
 */
static void run_at_a_box(walker *w, walker_area *area, int height, int depth)
{
	const world_point at = {1000, 0};

	walker_area_clear(area);
	walker_area_add_roof(area, -2000, -1000, 2000, 1000, 0);
	walker_area_add_roof(area, 200 - depth, -1000, 200, 1000, height);
	walker_init_at(w, at, 0, 0x4000);
}

static body_state from_walker(const walker *w)
{
	const int mode = walker_move(w) == WALKER_MOVE_VAULT ? BODY_VAULT : walker_move(w) == WALKER_MOVE_CLIMB ? BODY_CLIMB : w->falling ? BODY_AIR : BODY_MOVING;

	return doing(mode, U(walker_eye(w) - walker_step_lag(w)), w->stride, w->forward);
}

// A climb, whatever the ledge's height (and a vault up onto something low and deep): both hands
// come to its top's edge, at your feet's height, and stay on that spot of it while you pull up
// past them; you end stood on it, arms hanging
static void climbing_your_hands_take_the_edge(void)
{
	static const int heights[] = {30, 60, 75, 90, 100};

	for (int h = 0; h < 5; ++h)
	{
		const walker_controls ahead = {0, 1, 0, 0, 0};
		walker w;
		walker_area area;
		body b;
		int frames, held = 0;

		run_at_a_box(&w, &area, heights[h], 600);
		body_init(&b);
		for (frames = 0; frames < 400 && walker_move(&w) == WALKER_MOVE_NONE; ++frames)
		{
			const body_state s = (walker_update(&w, ahead, &area), from_walker(&w));

			body_update(&b, &s);
		}
		CHECK_EQ(WALKER_MOVE_CLIMB, walker_move(&w));
		CHECK_EQ(heights[h], walker_height(&w)); // your feet are on the top already
		for (frames = 0; frames < 200 && walker_move(&w) != WALKER_MOVE_NONE; ++frames)
		{
			const body_state s = (walker_update(&w, ahead, &area), from_walker(&w));

			body_update(&b, &s);
			check_bones(&b);
			// On the top (the body's ground), while it's within the arms' reach of your shoulders:
			// from hanging under it till you're up over it
			if (s.mode == BODY_CLIMB && b.joint[BODY_SHOULDER].z > -U(18) && s.eye < U(BODY_CLIMB_UP))
			{
				CHECK(near(b.joint[BODY_WRIST].z, 0, U(2)));
				CHECK(near(b.joint[BODY_WRIST_R].z, 0, U(2)));
				CHECK(b.joint[BODY_WRIST].x < 0 && b.joint[BODY_WRIST_R].x > 0);
				held++;
			}
		}
		CHECK(heights[h] <= 40 || held >= 1); // (the eye's up in a few frames: walker.c, ease_eye)
		for (frames = 0; frames < 40; ++frames)
		{
			const walker_controls still = {0, 0, 0, 0, 0};
			const body_state s = (walker_update(&w, still, &area), from_walker(&w));

			body_update(&b, &s);
		}
		CHECK(b.joint[BODY_WRIST].z > U(25)); // stood on it, arms down
		CHECK(near(b.joint[BODY_ANKLE].z, U(2), 4));
	}
}

// A vault over something low, whatever its height and depth: the left hand's on its top as you
// take off, and stays on that spot as you go over (it falls behind you), then lets go
static void vaulting_a_hand_stays_on_the_top(void)
{
	static const int sizes[][2] = {{40, 30}, {24, 16}, {46, 60}, {30, 45}};

	for (int k = 0; k < 4; ++k)
	{
		const walker_controls ahead = {0, 1, 0, 0, 0};
		walker w;
		walker_area area;
		body b;
		int frames, on_top = 0, first_y = 0, let_go = 0;

		run_at_a_box(&w, &area, sizes[k][0], sizes[k][1]);
		body_init(&b);
		for (frames = 0; frames < 400 && walker_move(&w) == WALKER_MOVE_NONE; ++frames)
		{
			const body_state s = (walker_update(&w, ahead, &area), from_walker(&w));

			body_update(&b, &s);
		}
		CHECK_EQ(WALKER_MOVE_VAULT, walker_move(&w));
		CHECK_EQ(sizes[k][0], walker_height(&w));
		for (frames = 0; frames < 100 && walker_move(&w) == WALKER_MOVE_VAULT; ++frames)
		{
			const body_state s = (walker_update(&w, ahead, &area), from_walker(&w));

			body_update(&b, &s);
			check_bones(&b);
			if (s.mode == BODY_VAULT && b.plant > -U(BODY_PLANT_BEHIND))
			{
				// Down at the top (the body's ground: your feet are a hop above it), on your left:
				// on it while the shoulder's within the arm's reach of it, stretched down at it
				// once the eye's risen past that. And the spot's further behind you each frame
				CHECK(b.joint[BODY_WRIST].x < 0);
				if (b.joint[BODY_SHOULDER].z < U(24))
					CHECK(b.joint[BODY_WRIST].z < U(5));
				CHECK(b.joint[BODY_WRIST].z < b.joint[BODY_SHOULDER].z - U(10));
				if (on_top++ > 0)
					CHECK(b.hand[0].y < first_y);
				first_y = b.hand[0].y;
			}
			else if (b.plant <= -U(BODY_PLANT_BEHIND))
				let_go = 1;
			CHECK(b.joint[BODY_WRIST_R].z > b.joint[BODY_WRIST].z - U(4) || let_go); // the right's up
		}
		CHECK(on_top >= 2);
		CHECK_EQ(WALKER_MOVE_NONE, walker_move(&w)); // landed beyond it: the move's over
		CHECK_EQ(0, walker_height(&w));
	}
}

void run_body_tests(void)
{
	RUN(climbing_your_hands_take_the_edge);
	RUN(vaulting_a_hand_stays_on_the_top);
	RUN(bones_never_stretch);
	RUN(standing_is_even);
	RUN(running_pumps_the_arms_into_view);
	RUN(walking_keeps_the_arms_down);
	RUN(the_crash_throws_your_hands_out);
	RUN(lying_you_look_at_your_hand);
	RUN(getting_up_pushes_off_the_floor);
	RUN(sliding_your_legs_go_ahead);
	RUN(changes_ease);
}
