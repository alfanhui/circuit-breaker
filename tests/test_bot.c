#include "test.h"
#include "../src/game/bot.h"
#include "../src/game/arena.h"

// The bot's look-ahead at the cruise speed setup() picks: 7 * 30 = 210 units
#define LOOK (CYCLE_BASE_SPEED * BOT_LOOK_FRAMES)

static rider bot;
static bot_mind mind;
static const trail *field[2]; // the trails it can see: its own, and another
static rider rival;
static const rider *rivals[1] = {&rival};
static int rival_count; // 0: alone on the field

static void setup(void)
{
	cycle_set_cruise_speed(CYCLE_BASE_SPEED);
	arena_set_pieces(ARENA_PIECES_DEFAULT);
	bot_init(&mind, 12345);
	field[0] = &bot.wall;
	rival_count = 0;
}

// A rival standing at (x, y), facing heading (its trail isn't on the field)
static void add_rival(short x, short y, unsigned char heading)
{
	rider_init(&rival, x, y, heading);
	rival_count = 1;
}

// This frame's buttons, from everything set up on the field
static rider_controls think(int trail_count)
{
	return bot_think(&mind, &bot, rivals, rival_count, field, trail_count);
}

// The turn it makes in a moment that doesn't change (the bot standing still in its thoughts):
// thinks up to the slowest reaction time and returns the first turn pressed, or TURN_NONE.
// *frames (if not NULL): which think pressed it, 1 being the first
static int decide(int trail_count, int *frames)
{
	for (int i = 1; i <= BOT_REACTION_MAX + 1; ++i)
	{
		int turn = think(trail_count).turn;

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
	c = think(1);
	CHECK_EQ(TURN_NONE, c.turn);
	CHECK_EQ(0, c.boost); // nobody to chase: no boost
}

static void turns_before_the_arena_edge(void)
{
	const int limit = arena_player_limit();

	setup();
	// Heading north (-y), 100 from the edge: within its look-ahead
	rider_init(&bot, 0, (short)-(limit - 100), HEADING_NORTH);
	CHECK(decide(1, NULL) != TURN_NONE);
	// Further away than it looks at all (its side look): carries on. (Nearer than that, with more
	// room to the sides, it already turns: it plans ahead rather than waiting for danger)
	bot_init(&mind, 12345);
	rider_init(&bot, 0, (short)-(limit - CYCLE_BASE_SPEED * BOT_SIDE_LOOK_FRAMES - 10), HEADING_NORTH);
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

		if (rider_step(&bot, think(1), field, 1) != TURN_NONE)
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
	CHECK_EQ(TURN_NONE, think(1).turn); // noticed, not pressed
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
		rider_step(&a, bot_think(&mind_a, &a, NULL, 0, field_a, 1), field_a, 1);
		rider_step(&b, bot_think(&mind_b, &b, NULL, 0, field_b, 1), field_b, 1);
	}
	CHECK_EQ(a.bike.position.x, b.bike.position.x);
	CHECK_EQ(a.bike.position.y, b.bike.position.y);
	CHECK_EQ(a.wall.corner_count, b.wall.corner_count);
}

// Level with a rival 2000 off to its left (west is +x, facing north): turning in now, it would
// only reach their line behind them. So it races on, boosting, to get ahead first
static void races_ahead_of_a_rival_level_with_it(void)
{
	rider_controls c;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(2000, 0, HEADING_NORTH);
	c = think(1);
	CHECK_EQ(1, c.boost);
	CHECK_EQ(TURN_NONE, decide(1, NULL));
}

// 2000 ahead of a rival 1000 off to its left: further ahead than across, so it turns in, across
// their path
static void cuts_across_once_ahead(void)
{
	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(1000, 2000, HEADING_NORTH); // south of it (+y), so behind
	CHECK_EQ(TURN_LEFT, decide(1, NULL));
	// The same on the right
	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(-1000, 2000, HEADING_NORTH);
	CHECK_EQ(TURN_RIGHT, decide(1, NULL));
	// Only just ahead (less than across): not yet
	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(1000, 800, HEADING_NORTH);
	CHECK_EQ(TURN_NONE, decide(1, NULL));
}

// Further than it can see: it doesn't know they're there
static void ignores_a_rival_out_of_sight(void)
{
	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(BOT_SIGHT + 100, 0, HEADING_NORTH);
	CHECK_EQ(TURN_NONE, decide(1, NULL));
}

// Chasing never outweighs danger: a rival on its left, but a trail 100 away on that side
static void will_not_chase_into_a_trail(void)
{
	static trail wall;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(1000, 2000, HEADING_NORTH); // far enough ahead to cut across
	another_trail(&wall, 100, 500, 100, -1000);
	CHECK_EQ(TURN_NONE, decide(2, NULL));
}

// The rival's point well ahead, the way clear: it boosts. Not towards a wall
static void boosts_towards_a_rival_ahead(void)
{
	const int limit = arena_player_limit();

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(0, -1500, HEADING_NORTH);
	CHECK_EQ(1, think(1).boost);
	// The rival is ahead but past the wall, which is close: no boost (it turns instead)
	setup();
	rider_init(&bot, 0, (short)-(limit - 300), HEADING_NORTH);
	add_rival(0, (short)-limit, HEADING_NORTH);
	CHECK_EQ(0, think(1).boost);
}

// Chasing a rival far ahead for 10 s: it boosts again and again, letting go when the gauge runs
// dry so it refills, rather than holding on to an empty gauge (which keeps the refill off)
static void boosts_again_once_the_gauge_refills(void)
{
	int boosts = 0;
	int was_boosting = 0;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	for (int frame = 0; frame < 600; ++frame)
	{
		rider_controls c;

		add_rival(0, (short)(bot.bike.position.y - 3000), HEADING_NORTH); // always 3000 ahead
		c = think(1);
		if (c.boost && !was_boosting)
			boosts++;
		was_boosting = c.boost;
		rider_step(&bot, c, field, 1);
	}
	// A boost lasts 1.5 s, then 0.5 s pause and 3 s refill: a new one every 5 s
	CHECK(boosts >= 2);
}

// Far enough ahead to cut, but a trail already crosses their path before that point: they're
// cut off already, so it doesn't turn across (it used to weave back and forth across their line)
static void does_not_cut_a_rival_already_blocked(void)
{
	static trail wall;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(1000, 2000, HEADING_NORTH);
	another_trail(&wall, 500, 1000, 1500, 1000); // across their path, 1000 ahead of them
	CHECK_EQ(TURN_NONE, decide(2, NULL));
}

// Heading across a rival's line to cut them off: it boosts to get there sooner
static void boosts_while_cutting_across(void)
{
	setup();
	// Heading west (+x); the rival 1000 to the west is 2000 behind (south, +y), heading north:
	// far enough ahead to cut, and its line is straight ahead
	rider_init(&bot, 0, 0, HEADING_WEST);
	add_rival(1000, 2000, HEADING_NORTH);
	CHECK_EQ(1, think(1).boost);
}

// Nobody to chase, but a trail close beside it: it boosts out of the gap. Open both sides: no
static void boosts_out_of_a_squeeze(void)
{
	static trail wall;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	another_trail(&wall, 100, 500, 100, -3000); // 100 to its left, all the way along
	CHECK_EQ(1, think(2).boost);
	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	CHECK_EQ(0, think(1).boost);
}

// A rival straight ahead: it holds its line rather than weaving
static void does_not_weave(void)
{
	int turns = 0;

	setup();
	rider_init(&bot, 0, 0, HEADING_NORTH);
	add_rival(0, -3000, HEADING_NORTH);
	for (int frame = 0; frame < 120; ++frame)
		if (rider_step(&bot, think(1), field, 1) != TURN_NONE)
			turns++;
	CHECK_EQ(0, turns);
}

void run_bot_tests(void)
{
	RUN(races_ahead_of_a_rival_level_with_it);
	RUN(cuts_across_once_ahead);
	RUN(ignores_a_rival_out_of_sight);
	RUN(will_not_chase_into_a_trail);
	RUN(boosts_towards_a_rival_ahead);
	RUN(boosts_again_once_the_gauge_refills);
	RUN(does_not_cut_a_rival_already_blocked);
	RUN(boosts_while_cutting_across);
	RUN(boosts_out_of_a_squeeze);
	RUN(does_not_weave);
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
