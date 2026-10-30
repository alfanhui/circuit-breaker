#include "bot.h"
#include "arena.h"
#include "collision.h"

#define OPTION_STRAIGHT 0
#define OPTION_LEFT 1
#define OPTION_RIGHT 2

void bot_init(bot_mind *mind, unsigned int seed)
{
	mind->random = seed ? seed : 1; // xorshift sticks at 0 forever
	mind->reacting = 0;
	mind->planned = TURN_NONE;
	mind->thinking = 0;
	mind->calm = 0;
	mind->boosting = 0;
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

static int abs_int(int v)
{
	return v < 0 ? -v : v;
}

// Where it aims, to cut off the nearest rival it can see (bot.h): far along their path while
// it races to get ahead, or straight across to their line once it's far enough ahead. Returns
// 0 if it can't see any, or if their way is already blocked (nothing left to cut)
static int find_target(const rider *me, const rider *const *rivals, int rival_count,
					   const trail *const *trails, int trail_count, world_point *target)
{
	const cycle *nearest = 0;
	int nearest_distance = BOT_SIGHT + 1;
	int dx;
	int dy;
	int ahead;
	int across;

	for (int i = 0; i < rival_count; ++i)
	{
		const cycle *r = &rivals[i]->bike;
		// "Distance" across plus along: no square root, and fine for comparing
		int distance = abs_int(r->position.x - me->bike.position.x) + abs_int(r->position.y - me->bike.position.y);

		if (distance < nearest_distance)
		{
			nearest_distance = distance;
			nearest = r;
		}
	}
	if (!nearest)
		return 0;

	// Where the bot is from the rival, measured along their heading (ahead of them is +) and
	// across it. Their heading is one of N/E/S/W, so DX and DY are -1, 0 or +1: these just pick
	// and flip the right differences
	dx = WORLD_HEADING_DX(nearest->heading);
	dy = WORLD_HEADING_DY(nearest->heading);
	ahead = (me->bike.position.x - nearest->position.x) * dx + (me->bike.position.y - nearest->position.y) * dy;
	across = abs_int((me->bike.position.x - nearest->position.x) * dy - (me->bike.position.y - nearest->position.y) * dx);

	if (ahead > across + cycle_cruise_speed() * BOT_CUT_MARGIN_FRAMES)
	{
		// Far enough ahead to get there first. But if something already blocks their way before
		// that point (usually its own trail, just laid across it), they're cut off: done. Without
		// this it crossed their line again and again, in a square wave in front of them
		if (collision_free_distance(nearest->position, nearest->heading, ahead, arena_player_limit(), trails,
									trail_count) < ahead)
			return 0;
		// Otherwise aim for the point on their line straight across from it
		target->x = (short)(nearest->position.x + dx * ahead);
		target->y = (short)(nearest->position.y + dy * ahead);
	}
	else
	{
		// Not yet: race for a point far along their path, measured from whichever of them is
		// further along (from the rival, it could be behind a bot that's already a little ahead)
		int race = (ahead > 0 ? ahead : 0) + cycle_cruise_speed() * BOT_RACE_FRAMES;

		target->x = (short)(nearest->position.x + dx * race);
		target->y = (short)(nearest->position.y + dy * race);
	}
	return 1;
}

// How much heading this way closes on target: its distance along the heading (negative when
// it's behind), capped to +-limit so a far rival doesn't outweigh everything else
static int approach(const rider *me, int heading, world_point target, int limit)
{
	int along = WORLD_HEADING_DX(heading) * (target.x - me->bike.position.x) +
				WORLD_HEADING_DY(heading) * (target.y - me->bike.position.y);

	return along > limit ? limit : along < -limit ? -limit : along;
}

rider_controls bot_think(bot_mind *mind, const rider *me, const rider *const *rivals, int rival_count,
						 const trail *const *trails, int trail_count)
{
	rider_controls controls;
	// Distances from frames of travel: a multiply each, no division
	const int look = cycle_cruise_speed() * BOT_LOOK_FRAMES;
	const int side_look = cycle_cruise_speed() * BOT_SIDE_LOOK_FRAMES;
	const int turn_cost = cycle_cruise_speed() * BOT_TURN_COST_FRAMES;
	world_point target;
	int has_target;
	int danger_ahead;
	int best = OPTION_STRAIGHT;
	int scores[3];
	int straight_room = 0;
	int straight_approach = 0;

	controls.turn = TURN_NONE;
	controls.boost = 0;
	if (mind->calm > 0)
		mind->calm--;

	// Already reacting to something: wait out the reaction time, then press. Off the boost
	// meanwhile: it's about to turn
	if (mind->reacting > 0)
	{
		mind->boosting = 0;
		if (--mind->reacting == 0)
			controls.turn = mind->planned;
		return controls;
	}

	// Danger ahead is checked every frame; the rest only every few frames (or now, in danger)
	danger_ahead = free_ahead(me, me->bike.heading, look, trails, trail_count) < look;
	if (mind->thinking > 0)
		mind->thinking--;
	if (!danger_ahead && mind->thinking > 0)
	{
		controls.boost = mind->boosting;
		return controls;
	}
	mind->thinking = BOT_THINK_FRAMES;

	// Score each option. Headings go N, E, S, W turning right, so +1 is right and +3 (one back
	// round the 4) is left
	has_target = find_target(me, rivals, rival_count, trails, trail_count, &target);
	for (int option = OPTION_STRAIGHT; option <= OPTION_RIGHT; ++option)
	{
		int heading = (me->bike.heading + (option == OPTION_LEFT ? 3 : option == OPTION_RIGHT ? 1 : 0)) & 3;
		int room = free_ahead(me, heading, side_look, trails, trail_count);
		int score = room * BOT_WEIGHT_ROOM;

		if (room < look)
			score -= (look - room) * BOT_WEIGHT_DANGER;
		if (has_target)
		{
			int a = approach(me, heading, target, side_look);

			score += a * BOT_WEIGHT_CHASE;
			if (option == OPTION_STRAIGHT)
				straight_approach = a;
		}
		if (option == OPTION_STRAIGHT)
			straight_room = room;
		else
			score -= turn_cost;
		scores[option] = score;
	}
	// The best, straight on winning ties (then left over right)
	if (scores[OPTION_LEFT] > scores[best])
		best = OPTION_LEFT;
	if (scores[OPTION_RIGHT] > scores[best])
		best = OPTION_RIGHT;
	// Calm: having just chosen a turn, it holds its line unless straight on is dangerous
	if (best != OPTION_STRAIGHT && mind->calm > 0 && !danger_ahead)
		best = OPTION_STRAIGHT;

	if (best == OPTION_STRAIGHT)
	{
		// Boost when the way is clear and the rival's point is well ahead (none to chase: no boost).
		// Like a player watching the gauge: it starts a boost only on a full gauge, and lets go once
		// it runs dry, so it refills (holding on, even empty, keeps the refill off: cycle.h)
		int wants = has_target && straight_room >= side_look && straight_approach * 2 > side_look;

		if (!mind->boosting)
			mind->boosting = wants && me->bike.gauge == CYCLE_GAUGE_MAX;
		else
			mind->boosting = wants && me->bike.gauge >= CYCLE_GAUGE_DRAIN;
		controls.boost = mind->boosting;
		return controls;
	}

	// A turn: it presses it a reaction time later. This frame counts as the first
	mind->boosting = 0;
	mind->planned = best == OPTION_LEFT ? TURN_LEFT : TURN_RIGHT;
	mind->calm = BOT_CALM_FRAMES;
	mind->reacting = (unsigned char)(BOT_REACTION_MIN - 1 + (next_random(mind) & BOT_REACTION_SPREAD_MASK));
	if (mind->reacting == 0)
		controls.turn = mind->planned; // (only if BOT_REACTION_MIN were 1)
	return controls;
}
