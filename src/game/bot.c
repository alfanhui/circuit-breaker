#include "bot.h"
#include "arena.h"
#include "collision.h"

// How far the bot at me could drive facing heading, looking up to distance: the same question
// its cycle asks every frame before moving (collision.h)
static int free_ahead(const rider *me, int heading, int distance, const trail *const *trails, int trail_count)
{
	return collision_free_distance(me->bike.position, heading, distance, arena_player_limit(), trails, trail_count);
}

rider_controls bot_think(const rider *me, const trail *const *trails, int trail_count)
{
	rider_controls controls;
	// Look-ahead distances: a multiply each, no division
	int look = cycle_cruise_speed() * BOT_LOOK_FRAMES;
	int side_look = cycle_cruise_speed() * BOT_SIDE_LOOK_FRAMES;
	int left;
	int right;

	controls.turn = TURN_NONE;
	controls.boost = 0;

	// Clear ahead: carry on
	if (free_ahead(me, me->bike.heading, look, trails, trail_count) >= look)
		return controls;

	// Something's coming: turn to the side with more room. Headings go N, E, S, W turning right,
	// so +1 is right and +3 (one back round the 4) is left
	left = free_ahead(me, (me->bike.heading + 3) & 3, side_look, trails, trail_count);
	right = free_ahead(me, (me->bike.heading + 1) & 3, side_look, trails, trail_count);
	controls.turn = right > left ? TURN_RIGHT : TURN_LEFT; // a tie goes left
	return controls;
}
