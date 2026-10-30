#include "test.h"
#include "../src/game/bot.h"
#include "../src/game/arena.h"

// The bot's look-ahead at the cruise speed setup() picks: 7 * 30 = 210 units
#define LOOK (CYCLE_BASE_SPEED * BOT_LOOK_FRAMES)

static rider bot;
static bot_mind mind;
static const trail *field[2]; // the trails it can see: its own, and another

static void setup(void)
{
	cycle_set_cruise_speed(CYCLE_BASE_SPEED);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
	bot_init(&mind, 12345);
	field[0] = &bot.wall;
}

// The turn it makes in a moment that doesn't change (the bot standing still in its thoughts):
// thinks up to the slowest reaction time and returns the first turn pressed, or TURN_NONE.
// *frames (if not NULL): which think pressed it, 1 being the first
static int decide(int trail_count, int *frames)
{
	for (int i = 1; i <= BOT_REACTION_MAX + 1; ++i)
	{
		int turn = bot_think(&mind, &bot, field, trail_count).turn;

		if (turn != TURN_NONE)
		{
			if (frames)
				*frames = i;
			return turn;
		}
	}
	return TURN_NONE;
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
	c = bot_think(&mind, &bot, field, 1);
	CHECK_EQ(TURN_NONE, c.turn);
	CHECK_EQ(0, c.boost); // no boost yet
}

static void turns_before_the_arena_edge(void)
{
	const int limit = arena_player_limit();

	setup();
	// Heading north (-y), 100 from the edge: within its look-ahead
	rider_init(&bot, 0, (short)-(limit - 100), HEADING_NORTH);
	CHECK(decide(1, NULL) != TURN_NONE);
	// Further away than it looks: carries on
	bot_init(&mind, 12345);
	rider_init(&bot, 0, (short)-(limit - LOOK - 10), HEADING_NORTH);
	CHECK_EQ(TURN_NONE, decide(1, NULL));
}

static void turns_towards_more_room(void)
{
	const int limit = arena_player_limit();

	setup();
	// Near the north edge, 300 from the west edge (+x). Facing north, west is on its left:
	// little room there, so it goes right (east)
	rider_init(&bot, (short)(limit - 300), (short)-(limit - 100), HEADING_NORTH);
	CHECK_EQ(TURN_RIGHT, decide(1, NULL));
	// The mirror image: 300 from the east edge, so it goes left
	bot_init(&mind, 12345);
	rider_init(&bot, (short)-(limit - 300), (short)-(limit - 100), HEADING_NORTH);
	CHECK_EQ(TURN_LEFT, decide(1, NULL));
}

static void turns_away_from_a_trail_ahead(void)
{
	static trail wall;
	world_point down;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	// A trail across its path 150 ahead (west is +x, on its left)...
	another_trail(&wall, -100, -150, 300, -150);
	// ...that turns and runs back down its left side, 300 away. Only the right is open
	trail_add_corner(&wall);
	down.x = 300;
	down.y = 500;
	trail_set_head(&wall, down);
	CHECK_EQ(TURN_RIGHT, decide(2, NULL));
}

static void ignores_what_is_far_away(void)
{
	static trail wall;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	another_trail(&wall, -1000, -1000, 1000, -1000);
	CHECK_EQ(TURN_NONE, decide(2, NULL));
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

		if (rider_step(&bot, bot_think(&mind, &bot, field, 1), field, 1) != TURN_NONE)
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

// Seeing a wall ahead, it doesn't turn that frame: it takes a human reaction time
static void reacts_after_a_human_delay(void)
{
	const int limit = arena_player_limit();
	int frames = 0;

	setup();
	rider_init(&bot, 0, (short)-(limit - 150), HEADING_NORTH);
	CHECK_EQ(TURN_NONE, bot_think(&mind, &bot, field, 1).turn); // noticed, not pressed
	bot_init(&mind, 12345);
	CHECK(decide(1, &frames) != TURN_NONE);
	CHECK(frames >= BOT_REACTION_MIN);
	CHECK(frames <= BOT_REACTION_MAX);
}

// Not the same time every time, but always within the range
static void reaction_times_vary(void)
{
	const int limit = arena_player_limit();
	int seen[BOT_REACTION_MAX + 2] = {0};
	int different = 0;

	for (unsigned int seed = 1; seed <= 200; ++seed)
	{
		int frames = 0;

		setup();
		bot_init(&mind, seed);
		rider_init(&bot, 0, (short)-(limit - 150), HEADING_NORTH);
		decide(1, &frames);
		CHECK(frames >= BOT_REACTION_MIN && frames <= BOT_REACTION_MAX);
		if (frames >= 0 && frames <= BOT_REACTION_MAX + 1)
			seen[frames]++;
	}
	for (int f = BOT_REACTION_MIN; f <= BOT_REACTION_MAX; ++f)
		if (seen[f] > 0)
			different++;
	CHECK(different >= 6); // most of the 8 possible times turn up in 200 tries
}

// The same seed is the same bot, frame for frame: runs can be repeated exactly
static void same_seed_same_bot(void)
{
	rider a;
	rider b;
	bot_mind mind_a;
	bot_mind mind_b;
	const trail *field_a[1];
	const trail *field_b[1];

	setup();
	rider_init(&a, 0, 0, HEADING_NORTH);
	rider_init(&b, 0, 0, HEADING_NORTH);
	bot_init(&mind_a, 99);
	bot_init(&mind_b, 99);
	field_a[0] = &a.wall;
	field_b[0] = &b.wall;
	for (int frame = 0; frame < 60 * 60; ++frame)
	{
		rider_step(&a, bot_think(&mind_a, &a, field_a, 1), field_a, 1);
		rider_step(&b, bot_think(&mind_b, &b, field_b, 1), field_b, 1);
	}
	CHECK_EQ(a.bike.position.x, b.bike.position.x);
	CHECK_EQ(a.bike.position.y, b.bike.position.y);
	CHECK_EQ(a.wall.corner_count, b.wall.corner_count);
}

void run_bot_tests(void)
{
	RUN(reacts_after_a_human_delay);
	RUN(reaction_times_vary);
	RUN(same_seed_same_bot);
	RUN(drives_straight_in_open_space);
	RUN(turns_before_the_arena_edge);
	RUN(turns_towards_more_room);
	RUN(turns_away_from_a_trail_ahead);
	RUN(ignores_what_is_far_away);
	RUN(survives_a_minute_alone);
}
