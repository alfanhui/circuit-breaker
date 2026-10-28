#include "cycle.h"
#include "arena.h"
#include "collision.h"

// Shared by every cycle: speed is a rule of the race, not a property of one bike
static int cruise_speed = CYCLE_BASE_SPEED;

int cycle_set_cruise_speed(int speed)
{
	if (speed < 1)
		speed = 1;
	if (speed > CYCLE_CRUISE_MAX)
		speed = CYCLE_CRUISE_MAX;
	cruise_speed = speed;
	return speed;
}

int cycle_cruise_speed(void)
{
	return cruise_speed;
}

void cycle_init(cycle *c, short x, short y, unsigned char heading)
{
	c->position.x = x;
	c->position.y = y;
	c->heading = heading & 3;
	c->speed = 1;
	c->boost = 0;
	c->bounded = 1;
	c->gauge = CYCLE_GAUGE_MAX;
	c->refill_wait = 0;
}

void cycle_turn_left(cycle *c)
{
	c->heading = (c->heading + 3) & 3;
}

void cycle_turn_right(cycle *c)
{
	c->heading = (c->heading + 1) & 3;
}

// Keeps one coordinate within -limit..limit. Returns 1 if it had to be moved back.
static int clamp_coordinate(short *value, short limit)
{
	if (*value > limit)
	{
		*value = limit;
		return 1;
	}
	if (*value < -limit)
	{
		*value = -limit;
		return 1;
	}
	return 0;
}

int cycle_update(cycle *c, int boosting, const trail *const *trails, int trail_count)
{
	int distance;
	int allowed;
	int clamped;
	short limit;

	// Ramp up to cruise from a standing start; drop straight to it if cruise was tuned down
	if (c->speed < cruise_speed)
		c->speed++;
	else if (c->speed > cruise_speed)
		c->speed = (unsigned char)cruise_speed;

	// Holding the button, even on an empty gauge, holds the refill off
	if (boosting)
		c->refill_wait = CYCLE_REFILL_DELAY;

	if (boosting && c->gauge >= CYCLE_GAUGE_DRAIN)
	{
		c->gauge -= CYCLE_GAUGE_DRAIN;
		c->boost = (c->boost + CYCLE_BOOST_RAMP_UP > CYCLE_BOOST_FULL) ? CYCLE_BOOST_FULL : c->boost + CYCLE_BOOST_RAMP_UP;
	}
	else
	{
		c->boost = (c->boost > CYCLE_BOOST_RAMP_DOWN) ? c->boost - CYCLE_BOOST_RAMP_DOWN : 0;
		if (c->refill_wait > 0)
			c->refill_wait--;
		else if (c->gauge < CYCLE_GAUGE_MAX)
			c->gauge = (c->gauge + CYCLE_GAUGE_REFILL > CYCLE_GAUGE_MAX) ? CYCLE_GAUGE_MAX : c->gauge + CYCLE_GAUGE_REFILL;
	}

	// The boost adds a share of the current speed: speed * share / 16 * level / 16, as one shift,
	// rounded to the nearest unit (+ 128 is half of 256). Rounded down, the first boosted frame
	// at cruise 7 added 0.66 of a unit: nothing
	distance = c->speed + ((c->speed * CYCLE_BOOST_SHARE * c->boost + 128) >> 8);

	// Ask before moving how far it can go: the arena edge and trails (collision.h). Unbounded
	// (debug, Y) is a ghost: through trails and out of the arena, up to the short overflow guard
	limit = (short)(c->bounded ? arena_player_limit() : WORLD_COORD_LIMIT);
	allowed = collision_free_distance(c->position, c->heading, distance, limit,
								   c->bounded ? trails : 0, c->bounded ? trail_count : 0);
	c->position.x = (short)(c->position.x + WORLD_HEADING_DX(c->heading) * allowed);
	c->position.y = (short)(c->position.y + WORLD_HEADING_DY(c->heading) * allowed);

	// A cycle left outside the arena (it shrank, or the boundary came back on) is pulled to the
	// edge. Only needed on the axis it isn't moving along: collision_edge already held the other
	clamped = clamp_coordinate(&c->position.x, limit);
	clamped |= clamp_coordinate(&c->position.y, limit);

	// A crash, for now, just stops it dead: the speed builds back up from 0 once it turns away
	if (allowed < distance || clamped)
	{
		c->speed = 0;
		c->boost = 0;
		return 1;
	}
	return 0;
}
