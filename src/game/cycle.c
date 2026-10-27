#include "cycle.h"

void cycle_init(cycle *c, short x, short y, unsigned char heading)
{
	c->position.x = x;
	c->position.y = y;
	c->heading = heading & 3;
	c->speed = 1;
	c->boost = 0;
	c->bounded = 1;
	c->gauge = CYCLE_GAUGE_MAX;
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

int cycle_update(cycle *c, int boosting)
{
	int distance;
	int clamped;
	short limit;

	if (c->speed < CYCLE_BASE_SPEED)
		c->speed++;

	if (boosting && c->gauge >= CYCLE_GAUGE_DRAIN)
	{
		c->gauge -= CYCLE_GAUGE_DRAIN;
		c->boost = (c->boost + CYCLE_BOOST_RAMP_UP > CYCLE_BOOST_MAX) ? CYCLE_BOOST_MAX : c->boost + CYCLE_BOOST_RAMP_UP;
	}
	else
	{
		c->boost = (c->boost > CYCLE_BOOST_RAMP_DOWN) ? c->boost - CYCLE_BOOST_RAMP_DOWN : 0;
		if (!boosting && c->gauge < CYCLE_GAUGE_MAX)
			c->gauge += CYCLE_GAUGE_REFILL;
	}

	distance = c->speed + c->boost;
	c->position.x = (short)(c->position.x + WORLD_HEADING_DX(c->heading) * distance);
	c->position.y = (short)(c->position.y + WORLD_HEADING_DY(c->heading) * distance);

	// The clamp happens in the same frame as the move, so the cycle never draws past the edge
	limit = c->bounded ? WORLD_PLAYER_LIMIT : WORLD_COORD_LIMIT;
	clamped = clamp_coordinate(&c->position.x, limit);
	clamped |= clamp_coordinate(&c->position.y, limit);
	return clamped;
}
