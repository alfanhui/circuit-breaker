#include "test.h"
#include "../src/game/target.h"

/*
 * Targeting on foot (game/target.h). The eye stands at (0, 0), 68 up, facing north (-y), so east
 * (-x) is on its right and west (+x) on its left
 */

static target_eye eye_at(int x, int y, int yaw)
{
	target_eye e;

	e.x = x;
	e.y = y;
	e.z = 68;
	e.yaw = yaw;
	e.view_yaw = yaw;
	return e;
}

static int near(int a, int b, int within)
{
	const int d = a - b;

	return d <= within && d >= -within;
}

// The bearing goes with walker.h's yaw: 0 north, a quarter east, half south, three quarters west
static void bearings_go_with_the_walkers_yaw(void)
{
	CHECK_EQ(0, target_bearing(0, -100));
	CHECK_EQ(0x4000, target_bearing(-100, 0));
	CHECK_EQ(0x8000, target_bearing(0, 100));
	CHECK_EQ(0xC000, target_bearing(100, 0));
	CHECK(near(target_bearing(-100, -100), 0x2000, 4)); // north-east
	CHECK(near(target_bearing(-100, -173), 0x1555, 24)); // 30 degrees right of north
	CHECK(near(target_bearing(173, -100), 0xC000 + 0x1555, 24)); // 30 degrees north of west
}

// Up is +, down -; level is 0. Within a degree or two (the floor's length is estimated)
static void pitch_is_up_or_down(void)
{
	CHECK_EQ(0, target_pitch(0, 0, -500));
	CHECK(near(target_pitch(500, 0, -500), 0x2000, 8));
	CHECK(near(target_pitch(-500, 0, -500), -0x2000, 8));
	CHECK(near(target_pitch(500, -500, -500), 0x1913, 0x100)); // 500 up, 707 along the floor: 35.3 degrees
}

// X takes the target nearest the view's middle on its left, Z on its right: the nearest to the
// middle, not the nearest to you
static void x_and_z_lock_left_and_right(void)
{
	target_list list;
	target_lock lock;
	const target_eye e = eye_at(0, 0, 0);

	target_list_clear(&list);
	target_list_add(&list, 300, -1000, 40, TARGET_SIGHT);	   // 0: left
	target_list_add(&list, -400, -400, 40, TARGET_SIGHT);	   // 1: right, near you but 45 degrees off
	target_list_add(&list, -100, -2000, 40, TARGET_CLUSTER); // 2: right, far but nearly ahead
	target_lock_init(&lock);

	CHECK(target_press_side(&lock, &list, &e, 0, 1));
	CHECK_EQ(2, lock.locked);
	CHECK(!target_press_side(&lock, &list, &e, 0, -1)); // pressed again (either): let go
	CHECK_EQ(TARGET_NONE, lock.locked);
	CHECK(target_press_side(&lock, &list, &e, 0, -1));
	CHECK_EQ(0, lock.locked);
}

// Nothing on the side pressed: nothing locked
static void nothing_that_side_locks_nothing(void)
{
	target_list list;
	target_lock lock;
	const target_eye e = eye_at(0, 0, 0);

	target_list_clear(&list);
	target_list_add(&list, -500, -1000, 40, TARGET_SIGHT); // right
	target_lock_init(&lock);
	CHECK(!target_press_side(&lock, &list, &e, 0, -1));
	CHECK_EQ(TARGET_NONE, lock.locked);
}

// Locked, the d-pad goes round to the next target that way, and stays at the last
static void the_d_pad_switches_round(void)
{
	target_list list;
	target_lock lock;
	const target_eye e = eye_at(0, 0, 0);

	target_list_clear(&list);
	target_list_add(&list, 500, -1000, 40, TARGET_SIGHT);  // 0: left
	target_list_add(&list, 0, -1000, 40, TARGET_SIGHT);	   // 1: ahead
	target_list_add(&list, -500, -1000, 40, TARGET_SIGHT); // 2: right
	target_lock_init(&lock);
	target_press_side(&lock, &list, &e, 0, 1);
	CHECK_EQ(1, lock.locked); // dead ahead is on both sides
	target_press_switch(&lock, &list, &e, 0, 1);
	CHECK_EQ(2, lock.locked);
	target_press_switch(&lock, &list, &e, 0, 1);
	CHECK_EQ(2, lock.locked); // nothing further right: stays
	target_press_switch(&lock, &list, &e, 0, -1);
	CHECK_EQ(1, lock.locked);
	target_press_switch(&lock, &list, &e, 0, -1);
	CHECK_EQ(0, lock.locked);

	// Unlocked, the d-pad does nothing here (it turns you: input.c)
	target_lock_init(&lock);
	target_press_switch(&lock, &list, &e, 0, 1);
	CHECK_EQ(TARGET_NONE, lock.locked);
}

// With an enemy in sight, it's all you can lock onto: either button takes it, and switching
// goes only between enemies. Hidden, the rest are back
static void an_enemy_takes_every_lock(void)
{
	target_list list;
	target_lock lock;
	walker_area area;
	const target_eye e = eye_at(0, 0, 0);

	target_list_clear(&list);
	target_list_add(&list, -300, -1000, 40, TARGET_SIGHT);	// 0: right
	target_list_add(&list, 600, -1000, 200, TARGET_ENEMY);	// 1: left
	target_list_add(&list, 1200, -1000, 200, TARGET_ENEMY); // 2: further left
	target_lock_init(&lock);
	CHECK(target_press_side(&lock, &list, &e, 0, 1)); // Z, the right: the enemy on the left all the same
	CHECK_EQ(1, lock.locked);
	target_press_switch(&lock, &list, &e, 0, 1); // right: only the sight that way, not an enemy
	CHECK_EQ(1, lock.locked);
	target_press_switch(&lock, &list, &e, 0, -1);
	CHECK_EQ(2, lock.locked);

	// Both enemies behind a tower: the sight's yours again
	walker_area_clear(&area);
	walker_area_add_roof(&area, -2000, -2000, 2000, 2000, 0);
	walker_area_add_roof(&area, 300, -700, 1600, -600, 400); // across the way to both
	target_lock_init(&lock);
	CHECK(target_press_side(&lock, &list, &e, &area, 1));
	CHECK_EQ(0, lock.locked);
}

// Sight: a tower between hides it; a box no taller than the lower of you and it doesn't; nor
// does the roof you stand on, looking down off it
static void towers_hide_targets(void)
{
	walker_area area;
	target_eye e = eye_at(0, 0, 0);
	target_marker m = {0, -1000, 40, TARGET_SIGHT};

	walker_area_clear(&area);
	walker_area_add_roof(&area, -2000, -2000, 2000, 2000, 0);
	CHECK(target_in_sight(&area, &e, &m));
	walker_area_add_roof(&area, -100, -600, 100, -500, 300); // a tower between
	CHECK(!target_in_sight(&area, &e, &m));
	m.x = 800; // off to the side of it
	CHECK(target_in_sight(&area, &e, &m));

	walker_area_clear(&area);
	walker_area_add_roof(&area, -2000, -2000, 2000, 2000, 0);
	walker_area_add_roof(&area, -100, -600, 100, -500, 30); // a kerb lower than the target's top
	m.x = 0;
	CHECK(target_in_sight(&area, &e, &m));

	// Stood on a platform 160 up, looking down past its edge at the crate
	walker_area_clear(&area);
	walker_area_add_roof(&area, -2000, -2000, 2000, 2000, 0);
	walker_area_add_roof(&area, -300, -300, 300, 300, 160);
	e.z = 160 + 68;
	CHECK(target_in_sight(&area, &e, &m));
}

// Too far, behind you, or underfoot: can't be locked. Locked, turning your body past its reach
// (or walking out of range) lets go at once; out of sight, it holds on half a second first
static void the_lock_lets_go(void)
{
	target_list list;
	target_lock lock;
	walker_area area;
	target_eye e = eye_at(0, 0, 0);

	target_list_clear(&list);
	target_list_add(&list, 0, 1000, 40, TARGET_SIGHT);				  // behind
	target_list_add(&list, 0, -(TARGET_RANGE + 100), 40, TARGET_SIGHT); // too far
	target_list_add(&list, 0, -30, 40, TARGET_SIGHT);				  // underfoot
	target_lock_init(&lock);
	CHECK(!target_press_side(&lock, &list, &e, 0, 1));
	CHECK(!target_press_side(&lock, &list, &e, 0, -1));

	target_list_clear(&list);
	target_list_add(&list, 0, -1000, 40, TARGET_SIGHT);
	CHECK(target_press_side(&lock, &list, &e, 0, 1));
	e.yaw = e.view_yaw = 0x4000 + 0x0400; // turned 95 degrees right
	target_frame(&lock, &list, &e, 0);
	CHECK_EQ(TARGET_NONE, lock.locked);

	e = eye_at(0, 0, 0);
	CHECK(target_press_side(&lock, &list, &e, 0, 1));
	e.y = 1600; // walked back: 2600 off
	target_frame(&lock, &list, &e, 0);
	CHECK_EQ(TARGET_NONE, lock.locked);

	// Behind a tower: held for TARGET_BLIND_FRAMES, then let go
	e = eye_at(0, 0, 0);
	CHECK(target_press_side(&lock, &list, &e, 0, 1));
	walker_area_clear(&area);
	walker_area_add_roof(&area, -2000, -2000, 2000, 2000, 0);
	walker_area_add_roof(&area, -100, -600, 100, -500, 300);
	for (int i = 0; i < TARGET_BLIND_FRAMES; ++i)
		target_frame(&lock, &list, &e, &area);
	CHECK_EQ(0, lock.locked);
	target_frame(&lock, &list, &e, &area);
	CHECK_EQ(TARGET_NONE, lock.locked);
}

// The head's aim: its turn from where the body faces (+ right) and its pitch (+ up)
static void the_aim_turns_the_head(void)
{
	target_list list;
	target_lock lock;
	target_eye e = eye_at(0, 0, 0);
	int head_yaw = 99, head_pitch = 99;

	target_list_clear(&list);
	target_list_add(&list, -1000, -1000, 68, TARGET_SIGHT); // 45 right, level with the eye
	target_list_add(&list, 1000, -1000, 68 - 1000, TARGET_SIGHT); // 45 left, well below
	target_lock_init(&lock);
	CHECK(!target_aim(&lock, &list, &e, &head_yaw, &head_pitch));

	CHECK(target_press_side(&lock, &list, &e, 0, 1));
	CHECK(target_aim(&lock, &list, &e, &head_yaw, &head_pitch));
	CHECK(near(head_yaw, 0x2000, 8));
	CHECK_EQ(0, head_pitch);

	// The body turned 30 degrees right: the head turns 30 less
	e.yaw = e.view_yaw = 0x1555;
	CHECK(target_aim(&lock, &list, &e, &head_yaw, &head_pitch));
	CHECK(near(head_yaw, 0x2000 - 0x1555, 8));

	e = eye_at(0, 0, 0);
	target_lock_init(&lock);
	CHECK(target_press_side(&lock, &list, &e, 0, -1));
	CHECK(target_aim(&lock, &list, &e, &head_yaw, &head_pitch));
	CHECK(near(head_yaw, -0x2000, 8));
	CHECK(head_pitch < -0x1000); // down: ~35 degrees
}

void run_target_tests(void)
{
	RUN(bearings_go_with_the_walkers_yaw);
	RUN(pitch_is_up_or_down);
	RUN(x_and_z_lock_left_and_right);
	RUN(nothing_that_side_locks_nothing);
	RUN(the_d_pad_switches_round);
	RUN(an_enemy_takes_every_lock);
	RUN(towers_hide_targets);
	RUN(the_lock_lets_go);
	RUN(the_aim_turns_the_head);
}
