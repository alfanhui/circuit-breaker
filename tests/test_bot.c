#include "test.h"
#include "../src/game/bot.h"
#include "../src/game/arena.h"

// The bot's look-ahead at the cruise speed setup() picks: 7 * 30 = 210 units
#define LOOK (CYCLE_BASE_SPEED * BOT_LOOK_FRAMES)

static rider bot;
static const trail *field[2]; // the trails it can see: its own, and another

static void setup(void)
{
	cycle_set_cruise_speed(CYCLE_BASE_SPEED);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
	field[0] = &bot.wall;
}

// Another trail on the field, from (ax, ay) to (bx, by): one straight segment
static void another_trail(trail *t, short ax, short ay, short bx, short by)
{
	world_point a;
	world_point b;

	a.x = ax;
	a.y = ay;
	b.x = bx;
	b.y = by;
	trail_reset(t, a);
	trail_set_head(t, b);
	field[1] = t;
}

static void drives_straight_in_open_space(void)
{
	rider_controls c;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	c = bot_think(&bot, field, 1);
	CHECK_EQ(TURN_NONE, c.turn);
	CHECK_EQ(0, c.boost); // no boost in step 1
}

static void turns_before_the_arena_edge(void)
{
	const int limit = arena_player_limit();

	setup();
	// Heading north (-y), 100 from the edge: within its look-ahead
	rider_init(&bot, 0, (short)-(limit - 100), HEADING_NORTH);
	CHECK(bot_think(&bot, field, 1).turn != TURN_NONE);
	// Further away than it looks: carries on
	rider_init(&bot, 0, (short)-(limit - LOOK - 10), HEADING_NORTH);
	CHECK_EQ(TURN_NONE, bot_think(&bot, field, 1).turn);
}

static void turns_towards_more_room(void)
{
	const int limit = arena_player_limit();

	setup();
	// Near the north edge, 300 from the west edge (+x). Facing north, west is on its left:
	// little room there, so it goes right (east)
	rider_init(&bot, (short)(limit - 300), (short)-(limit - 100), HEADING_NORTH);
	CHECK_EQ(TURN_RIGHT, bot_think(&bot, field, 1).turn);
	// The mirror image: 300 from the east edge, so it goes left
	rider_init(&bot, (short)-(limit - 300), (short)-(limit - 100), HEADING_NORTH);
	CHECK_EQ(TURN_LEFT, bot_think(&bot, field, 1).turn);
}

static void turns_away_from_a_trail_ahead(void)
{
	static trail wall;

	setup();
	world_point down;

	rider_init(&bot, 0, 0, HEADING_NORTH);
	// A trail across its path 150 ahead (west is +x, on its left)...
	another_trail(&wall, -100, -150, 300, -150);
	// ...that turns and runs back down its left side, 300 away. Only the right is open
	trail_add_corner(&wall);
	down.x = 300;
	down.y = 500;
	trail_set_head(&wall, down);
	CHECK_EQ(TURN_RIGHT, bot_think(&bot, field, 2).turn);
}

static void ignores_what_is_far_away(void)
{
	static trail wall;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	another_trail(&wall, -1000, -1000, 1000, -1000);
	CHECK_EQ(TURN_NONE, bot_think(&bot, field, 2).turn);
}

// The whole loop: the bot's buttons drive its rider through the same code as a player's.
// Alone in the arena for a minute, it should never crash
static void survives_a_minute_alone(void)
{
	int crashes = 0;
	int turns = 0;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	for (int frame = 0; frame < 60 * 60; ++frame)
	{
		int was_moving = bot.bike.speed > 0;

		if (rider_step(&bot, bot_think(&bot, field, 1), field, 1) != TURN_NONE)
			turns++;
		if (was_moving && bot.bike.speed == 0) // a crash stops it dead (cycle.h)
			crashes++;
	}
	CHECK_EQ(0, crashes);
	// It really went round rather than stopping at the first wall. A minute at cruise 7 is 25200
	// units: centre to the north wall (~7900), turn, along to the corner (~7900), turn, then most
	// of the way along the next wall (15800). So 2 turns
	CHECK(turns >= 2);
}

void run_bot_tests(void)
{
	RUN(drives_straight_in_open_space);
	RUN(turns_before_the_arena_edge);
	RUN(turns_towards_more_room);
	RUN(turns_away_from_a_trail_ahead);
	RUN(ignores_what_is_far_away);
	RUN(survives_a_minute_alone);
}
