#include "cycle.h"

void cycle_init(cycle *c, short x, short y, unsigned char heading)
{
	c->position.x = x;
	c->position.y = y;
	c->heading = heading & 3;
	c->speed = 1;
	c->boost = 0;
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

static int clamp_to_arena(short *value)
{
	if (*value > WORLD_PLAYER_LIMIT)
	{
		*value = WORLD_PLAYER_LIMIT;
		return 1;
	}
	if (*value < -WORLD_PLAYER_LIMIT)
	{
		*value = -WORLD_PLAYER_LIMIT;
		return 1;
	}
	return 0;
}

int cycle_update(cycle *c, int boosting)
{
	int distance;
	int clamped;

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

	clamped = clamp_to_arena(&c->position.x);
	clamped |= clamp_to_arena(&c->position.y);
	return clamped;
}
