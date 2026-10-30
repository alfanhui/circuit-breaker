#include "test.h"
#include "../src/game/rider.h"
#include "../src/game/arena.h"
#include "../src/game/collision.h"

static const rider_controls NOTHING = {TURN_NONE, 0};
static const rider_controls LEFT = {TURN_LEFT, 0};
static const rider_controls RIGHT = {TURN_RIGHT, 0};
static const rider_controls BOOST = {TURN_NONE, 1};

static void setup(void)
{
	cycle_set_cruise_speed(CYCLE_BASE_SPEED);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
}

// Its own trail only, as when riding alone
static int step(rider *r, rider_controls controls)
{
	const trail *own[1];

	own[0] = &r->wall;
	return rider_step(r, controls, own, 1);
}

static void it_moves_and_the_trail_follows(void)
{
	rider r;

	setup();
	rider_init(&r, 0, 0, HEADING_NORTH);
	for (int i = 0; i < 20; ++i)
		CHECK_EQ(TURN_NONE, step(&r, NOTHING));
	CHECK(r.bike.position.y < 0); // north is -y
	CHECK_EQ(r.bike.position.y, r.wall.head.y);
	CHECK_EQ(1, trail_segment_count(&r.wall));
}

static void a_turn_pins_a_corner(void)
{
	rider r;
	int corners;

	setup();
	rider_init(&r, 0, 0, HEADING_NORTH);
	for (int i = 0; i < 20; ++i)
		step(&r, NOTHING);
	corners = r.wall.corner_count;
	CHECK_EQ(TURN_LEFT, step(&r, LEFT));
	CHECK_EQ(HEADING_WEST, r.bike.heading); // left of north
	CHECK_EQ(corners + 1, r.wall.corner_count);
}

static void a_quick_second_turn_waits_for_the_gap(void)
{
	rider r;
	int frames = 0;

	setup();
	rider_init(&r, 0, 0, HEADING_NORTH);
	for (int i = 0; i < 20; ++i)
		step(&r, NOTHING);
	CHECK_EQ(TURN_RIGHT, step(&r, RIGHT));
	// Pressed again straight away: held back, not lost (turn_gate.h), then made once the cycle
	// has gone a cycle-width, without pressing again
	CHECK_EQ(TURN_NONE, step(&r, RIGHT));
	while (step(&r, NOTHING) == TURN_NONE && ++frames < 100)
		;
	CHECK(frames < 100);
	CHECK_EQ(HEADING_SOUTH, r.bike.heading); // a U-turn
}

static void boost_drains_the_gauge(void)
{
	rider r;

	setup();
	rider_init(&r, 0, 0, HEADING_NORTH);
	step(&r, BOOST);
	CHECK(r.bike.gauge < CYCLE_GAUGE_MAX);
}

static void another_riders_trail_stops_it(void)
{
	rider me;
	rider other;
	const trail *both[2];

	setup();
	rider_init(&me, 0, 0, HEADING_NORTH);
	// The other rider's trail lies across the way, 200 ahead
	rider_init(&other, -500, -200, HEADING_WEST);
	other.wall.head.x = 500;
	both[0] = &me.wall;
	both[1] = &other.wall;
	for (int i = 0; i < 100; ++i)
		rider_step(&me, NOTHING, both, 2);
	CHECK_EQ(-200 + COLLISION_TRAIL_CLEARANCE, me.bike.position.y); // stopped short of it
}

void run_rider_tests(void)
{
	RUN(it_moves_and_the_trail_follows);
	RUN(a_turn_pins_a_corner);
	RUN(a_quick_second_turn_waits_for_the_gap);
	RUN(boost_drains_the_gauge);
	RUN(another_riders_trail_stops_it);
}
