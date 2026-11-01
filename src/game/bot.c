#include "bot.h"
#include "arena.h"
#include "collision.h"

#define OPTION_STRAIGHT 0
#define OPTION_LEFT 1
#define OPTION_RIGHT 2

// Each difficulty's numbers (bot.h, where the choice of them is explained). Easy is the bot as
// it was first tuned: the macros in bot.h
const bot_skill bot_skills[BOT_SKILL_COUNT] = {
	// name    reaction          calm             cut margin             boost from            chase
	{"Easy", BOT_REACTION_MIN, BOT_CALM_FRAMES, BOT_CUT_MARGIN_FRAMES, CYCLE_GAUGE_MAX, BOT_WEIGHT_CHASE},
	{"Medium", 6, 20, BOT_CUT_MARGIN_FRAMES, CYCLE_GAUGE_MAX / 2, BOT_WEIGHT_CHASE},
	{"Hard", 3, 12, 12, CYCLE_GAUGE_MAX / 2, 2},
};

void bot_init(bot_mind *mind, unsigned int seed)
{
	mind->random = seed ? seed : 1; // xorshift sticks at 0 forever
	mind->reacting = 0;
	mind->planned = TURN_NONE;
	mind->thinking = 0;
	mind->calm = 0;
	mind->boosting = 0;
	mind->may_think = 1; // every frame, unless the game shares the turns out
	mind->in_danger = 0;
	mind->skill = BOT_EASY;
	mind->believes = 0;
	mind->unseen = 0;
	mind->rival_heading = 0;
	mind->rival_seen.x = 0;
	mind->rival_seen.y = 0;
}

void bot_set_skill(bot_mind *mind, int skill)
{
	mind->skill = (unsigned char)(skill >= 0 && skill < BOT_SKILL_COUNT ? skill : BOT_EASY);
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

// A way blocked within sight: is there a way out of it? From the last point it could still turn
// before what blocks it (look short of it: time to react), how far it could go to the more open
// side. A pocket (a corridor between trails, closed at the end) has none: riding in, it was
// trapped, and most of its deaths against a player weaving close by were into its own trail
static int way_out(const rider *me, int heading, int room, int look, int side_look, const trail *const *trails,
				   int trail_count)
{
	world_point at = me->bike.position;
	int run = room > look ? room - look : 0;
	int left;
	int right;

	at.x = (short)(at.x + WORLD_HEADING_DX(heading) * run);
	at.y = (short)(at.y + WORLD_HEADING_DY(heading) * run);
	left = collision_free_distance(at, (heading + 3) & 3, side_look, arena_player_limit(), trails, trail_count);
	right = collision_free_distance(at, (heading + 1) & 3, side_look, arena_player_limit(), trails, trail_count);
	return left > right ? left : right;
}

static int abs_int(int v)
{
	return v < 0 ? -v : v;
}

// Where it aims, to cut off the nearest rival (bot.h): far along their path while it races to
// get ahead, or straight across to their line once it's far enough ahead. A rival beyond
// BOT_SIGHT it can't see, only the way to them (as the player's radar shows the bots): it heads
// for them. Returns 0 if there's no rival, or if their way is already blocked (nothing to cut)
static int find_target(bot_mind *mind, const rider *me, const rider *const *rivals, int rival_count,
					   const trail *const *trails, int trail_count, world_point *target)
{
	const cycle *nearest = 0;
	int nearest_distance = 0;
	int dx;
	int dy;
	int ahead;
	int across;
	int side;
	world_point at; // where it believes the rival is, and which way they're going
	int heading;
	int travelled;

	for (int i = 0; i < rival_count; ++i)
	{
		const cycle *r = &rivals[i]->bike;
		// "Distance" across plus along: no square root, and fine for comparing
		int distance = abs_int(r->position.x - me->bike.position.x) + abs_int(r->position.y - me->bike.position.y);

		if (!nearest || distance < nearest_distance)
		{
			nearest_distance = distance;
			nearest = r;
		}
	}
	if (!nearest)
		return 0;
	if (nearest_distance > BOT_SIGHT)
	{
		// Too far to see, but the radar says which way: head for them. Only the way matters, as
		// approach() caps how far a point counts, so where exactly they are gives nothing away
		*target = nearest->position;
		return 1;
	}
	// Near, it sees them only in the half in front of it (a person's eyes, side vision
	// included): along its own heading, a sign and a subtraction. Behind it, it goes by what it
	// believes: they carried on the way it last saw them going, at cruise speed (bot.h). Before,
	// it knew which way a rival turned the moment they turned, and twisted after every turn of
	// a player weaving close behind it without having seen them. Never seen yet (a round's
	// start), it knows where everyone starts
	if (WORLD_HEADING_DX(me->bike.heading) * (nearest->position.x - me->bike.position.x) +
				WORLD_HEADING_DY(me->bike.heading) * (nearest->position.y - me->bike.position.y) >=
			0 ||
		!mind->believes)
	{
		mind->believes = 1;
		mind->unseen = 0;
		mind->rival_seen = nearest->position;
		mind->rival_heading = nearest->heading;
	}
	heading = mind->rival_heading;
	dx = WORLD_HEADING_DX(heading);
	dy = WORLD_HEADING_DY(heading);
	travelled = cycle_cruise_speed() * mind->unseen;
	// They can't have ridden through a wall or a trail: if that's where the belief has them,
	// they turned somewhere it didn't see, and it has lost them. Until it sees them again it
	// heads for the arena's centre, where there's most often room and it's likely to meet them.
	// (In a PC simulation, heading for them by the radar drove it into the knot of trails a
	// weaving player leaves; aiming nowhere, it wandered and spiralled into its own trail)
	if (travelled > 0 &&
		collision_free_distance(mind->rival_seen, heading, travelled, arena_player_limit(), trails, trail_count) <
			travelled)
	{
		target->x = 0;
		target->y = 0;
		return 1;
	}
	at.x = (short)(mind->rival_seen.x + dx * travelled);
	at.y = (short)(mind->rival_seen.y + dy * travelled);

	// Where the bot is from the rival, measured along their heading (ahead of them is +) and
	// across it. Their heading is one of N/E/S/W, so DX and DY are -1, 0 or +1: these just pick
	// and flip the right differences
	ahead = (me->bike.position.x - at.x) * dx + (me->bike.position.y - at.y) * dy;
	across = (me->bike.position.x - at.x) * dy - (me->bike.position.y - at.y) * dx;
	side = across < 0 ? -1 : 1; // which side of their line the bot is on
	across = abs_int(across);

	if (ahead > across + cycle_cruise_speed() * bot_skills[mind->skill].cut_margin_frames)
	{
		// Far enough ahead to get there first. But if something already blocks their way before
		// that point (usually its own trail, just laid across it), they're cut off: done. Without
		// this it crossed their line again and again, in a square wave in front of them
		if (collision_free_distance(at, heading, ahead, arena_player_limit(), trails, trail_count) < ahead)
			return 0;
		// Otherwise aim for the point on their line straight across from it
		target->x = (short)(at.x + dx * ahead);
		target->y = (short)(at.y + dy * ahead);
	}
	else
	{
		// Not yet: race for a point far along their path, measured from whichever of them is
		// further along (from the rival, it could be behind a bot that's already a little ahead)
		int race = (ahead > 0 ? ahead : 0) + cycle_cruise_speed() * BOT_RACE_FRAMES;

		// In a lane of its own, BOT_LANE to its side of their line. Aimed at their line itself,
		// it rode right beside their trail (~15 units off), and when they turned (at a wall,
		// say) their new trail was laid across its nose before it could react: a third of its
		// deaths against a player riding straight. (dy, -dx) points across their line, to the
		// side `across` is measured towards
		target->x = (short)(at.x + dx * race + dy * side * BOT_LANE);
		target->y = (short)(at.y + dy * race - dx * side * BOT_LANE);
	}
	// Kept a lane's width inside the arena. Racing a rival riding at a wall, it aimed past the
	// wall: it rode into the strip beside it, and when the rival turned along the wall their
	// trail closed the strip on it
	{
		int limit = arena_player_limit() - BOT_LANE;

		target->x = (short)(target->x > limit ? limit : target->x < -limit ? -limit : target->x);
		target->y = (short)(target->y > limit ? limit : target->y < -limit ? -limit : target->y);
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
	const bot_skill *skill = &bot_skills[mind->skill]; // how hard it plays (bot.h)
	// Distances from frames of travel: a multiply each, no division
	const int look = cycle_cruise_speed() * BOT_LOOK_FRAMES;
	const int side_look = cycle_cruise_speed() * BOT_SIDE_LOOK_FRAMES;
	const int turn_cost = cycle_cruise_speed() * BOT_TURN_COST_FRAMES;
	const int reaction_travel = cycle_cruise_speed() * (skill->reaction_min + BOT_REACTION_SPREAD_MASK);
	world_point target;
	int has_target;
	int danger_ahead;
	int any_safe = 0;
	int best = -1;
	int best_value = 0;
	int scores[3];
	int rooms[3];
	unsigned char safes[3];
	int straight_approach = 0;
	int correcting = 0;

	controls.turn = TURN_NONE;
	controls.boost = 0;
	if (mind->calm > 0)
		mind->calm--;
	if (mind->unseen < BOT_MEMORY_FRAMES)
		mind->unseen++; // reset whenever it sees them (find_target)

	// Already reacting to something: wait out the reaction time, then press. Off the boost
	// meanwhile: it's about to turn
	if (mind->reacting > 0)
	{
		int heading = (me->bike.heading + (mind->planned == TURN_LEFT ? 3 : 1)) & 3;

		mind->boosting = 0;
		if (--mind->reacting > 0)
			return controls;
		// Its hand is on the button: one last look that way. Pressing blindly, it turned into
		// trails laid across that way while it reacted (a player weaving close by does that
		// all the time). If it has closed, it decides again, and acts on that at once: it's a
		// correction of a move already under way, not a new one
		if (free_ahead(me, heading, look, trails, trail_count) >= look)
		{
			controls.turn = mind->planned;
			return controls;
		}
		correcting = 1;
		mind->calm = 0;
	}

	// Danger ahead is checked every frame: one question. The rest only every few frames, and on
	// its turn (bot.h: may_think), unless danger has just appeared: then it thinks now, turn or
	// not. Left to its turn, it noticed a closing wall up to 2 frames late, and in the PC
	// simulation crashed into its own trail nearly 3 times as often against a careless player.
	// Danger that stays (no safe way: it rides on, looking for one) is thought about on each of
	// its turns, not every frame: thinking again every frame of it, 3 bots boxed in among long
	// trails took nearly half a frame. (Only at the usual pace, every few frames, it found a way
	// out too late: 251 of 300 lived the minute against a careless player, against 276)
	danger_ahead = free_ahead(me, me->bike.heading, look, trails, trail_count) < look;
	{
		const int new_danger = danger_ahead && !mind->in_danger;
		const int due = mind->may_think && (mind->thinking <= 1 || danger_ahead); // its turn, and time

		mind->in_danger = (unsigned char)danger_ahead;
		if (mind->thinking > 0)
			mind->thinking--;
		if (!new_danger && !due && !correcting)
		{
			controls.boost = mind->boosting;
			return controls;
		}
	}
	mind->thinking = BOT_THINK_FRAMES;

	// Score each option. Headings go N, E, S, W turning right, so +1 is right and +3 (one back
	// round the 4) is left
	has_target = find_target(mind, me, rivals, rival_count, trails, trail_count, &target);
	for (int option = OPTION_STRAIGHT; option <= OPTION_RIGHT; ++option)
	{
		int heading = (me->bike.heading + (option == OPTION_LEFT ? 3 : option == OPTION_RIGHT ? 1 : 0)) & 3;
		int room = free_ahead(me, heading, side_look, trails, trail_count);
		int score = room * BOT_WEIGHT_ROOM;
		// A turn needs more: room for the next reaction too. With just the look-ahead, it turned
		// into a gap and was in danger again at once, zigzagging deeper into a tight space
		int safe = room >= (option == OPTION_STRAIGHT ? look : look + reaction_travel);

		// Blocked within sight: is there a way out near the end? A dead end is as unsafe as a
		// wall right ahead. A way out adds its room to the score (only the score: how far it
		// can really go straight, for safety and boosting, stays the distance to what blocks it)
		if (safe && room < side_look)
		{
			int out = way_out(me, heading, room, look, side_look, trails, trail_count);

			if (out < BOT_WAY_OUT_FRAMES * cycle_cruise_speed())
				safe = 0;
			else
				score += (out < side_look - room ? out : side_look - room) * BOT_WEIGHT_ROOM;
		}

		if (has_target)
		{
			int a = approach(me, heading, target, side_look);

			score += a * skill->chase_weight;
			if (option == OPTION_STRAIGHT)
				straight_approach = a;
		}
		if (option != OPTION_STRAIGHT)
			score -= turn_cost;
		rooms[option] = room;
		scores[option] = score;
		safes[option] = (unsigned char)safe;
		any_safe |= safe;
	}
	// Safety first. A way that hits something within the look-ahead is ruled out while there's a
	// safe one, however well it scores for the chase. Before, danger was only a penalty in the
	// score, and the chase's pull (up to side_look) outweighed it until the wall was ~100 units
	// off: less than the bot travels while it reacts, so it turned too late and crashed (most
	// of its deaths in a PC simulation against a player weaving close by). With no safe way, the
	// one with the most room wins. Ties: straight on, then left, then right
	for (int option = OPTION_STRAIGHT; option <= OPTION_RIGHT; ++option)
	{
		int value = any_safe ? scores[option] : rooms[option];

		if (any_safe && !safes[option])
			continue;
		if (best < 0 || value > best_value)
		{
			best = option;
			best_value = value;
		}
	}
	// Calm: having just chosen a turn, it holds its line unless straight on is dangerous
	if (best != OPTION_STRAIGHT && mind->calm > 0 && !danger_ahead)
		best = OPTION_STRAIGHT;

	if (best == OPTION_STRAIGHT)
	{
		// Boost, with the way ahead clear, when either:
		//   - where it aims is ahead of it: racing to get ahead of a rival (the only way to set up
		//     a cut against one riding straight, bot.h), or crossing their line to cut them off.
		//     Before, it boosted only for a point over 1 s ahead, so mostly when the rival had
		//     boosted away from it: it looked like it copied the player
		//   - it's squeezed: something close beside it, on either side. A burst gets it out of
		//     the gap before the gap closes
		// Like a player watching the gauge: it starts a boost only on a full gauge, and lets go once
		// it runs dry, so it refills (holding on, even empty, keeps the refill off: cycle.h)
		int clear = rooms[OPTION_STRAIGHT] >= side_look;
		int chasing = has_target && straight_approach > look;
		int squeezed = rooms[OPTION_LEFT] < look || rooms[OPTION_RIGHT] < look;
		int wants = clear && (chasing || squeezed);

		if (!mind->boosting)
			mind->boosting = wants && me->bike.gauge >= skill->boost_from;
		else
			mind->boosting = wants && me->bike.gauge >= CYCLE_GAUGE_DRAIN;
		controls.boost = mind->boosting;
		return controls;
	}

	// A turn: it presses it a reaction time later. This frame counts as the first
	mind->boosting = 0;
	mind->planned = best == OPTION_LEFT ? TURN_LEFT : TURN_RIGHT;
	mind->calm = skill->calm_frames;
	mind->reacting = (unsigned char)(skill->reaction_min - 1 + (next_random(mind) & BOT_REACTION_SPREAD_MASK));
	if (correcting)
		mind->reacting = 0; // already reacting: pressed now
	if (mind->reacting == 0)
		controls.turn = mind->planned; // (or if BOT_REACTION_MIN were 1)
	return controls;
}
