#include "bot.h"
#include "arena.h"
#include "collision.h"

void bot_init(bot_mind *mind, unsigned int seed)
{
	mind->random = seed ? seed : 1; // xorshift sticks at 0 forever
	mind->reacting = 0;
	mind->planned = TURN_NONE;
}

// The next number in the bot's random sequence. xorshift: three shifts and three exclusive ORs,
// no multiply or divide, and it runs through every 32-bit number except 0 before repeating.
// Good enough for reaction times; not for anything that needs real randomness
static unsigned int next_random(bot_mind *mind)
{
	unsigned int x = mind->random;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	mind->random = x;
	return x;
}

// How far the bot at me could drive facing heading, looking up to distance: the same question
// its cycle asks every frame before moving (collision.h)
static int free_ahead(const rider *me, int heading, int distance, const trail *const *trails, int trail_count)
{
	return collision_free_distance(me->bike.position, heading, distance, arena_player_limit(), trails, trail_count);
}

rider_controls bot_think(bot_mind *mind, const rider *me, const trail *const *trails, int trail_count)
{
	rider_controls controls;
	// Look-ahead distances: a multiply each, no division
	int look = cycle_cruise_speed() * BOT_LOOK_FRAMES;
	int side_look = cycle_cruise_speed() * BOT_SIDE_LOOK_FRAMES;
	int left;
	int right;

	controls.turn = TURN_NONE;
	controls.boost = 0;

	// Already reacting to something it saw: wait out the reaction time, then press
	if (mind->reacting > 0)
	{
		if (--mind->reacting == 0)
			controls.turn = mind->planned;
		return controls;
	}

	// Clear ahead: carry on
	if (free_ahead(me, me->bike.heading, look, trails, trail_count) >= look)
		return controls;

	// Something's coming. Decide now, from what it sees now, which side has more room. Headings
	// go N, E, S, W turning right, so +1 is right and +3 (one back round the 4) is left
	left = free_ahead(me, (me->bike.heading + 3) & 3, side_look, trails, trail_count);
	right = free_ahead(me, (me->bike.heading + 1) & 3, side_look, trails, trail_count);
	mind->planned = right > left ? TURN_RIGHT : TURN_LEFT; // a tie goes left
	// ...but its hands only move a reaction time later. This frame counts as the first
	mind->reacting = (unsigned char)(BOT_REACTION_MIN - 1 + (next_random(mind) & BOT_REACTION_SPREAD_MASK));
	if (mind->reacting == 0)
		controls.turn = mind->planned; // (only if BOT_REACTION_MIN were 1)
	return controls;
}
