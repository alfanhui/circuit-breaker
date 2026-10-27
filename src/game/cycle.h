#ifndef __CYCLE_H__
#define __CYCLE_H__

#include "world.h"

/*
 * Light cycle movement in world space. Pure C, integer only (the SH-2 has no FPU).
 */

#define CYCLE_BASE_SPEED 7		 // default world units per frame at cruise (tunable, see below)
#define CYCLE_CRUISE_MAX 40		 // 2400 units/s: crosses the largest arena in about 12 s
#define CYCLE_BOOST_MAX 7		 // extra units per frame at full boost
#define CYCLE_BOOST_RAMP_UP 4	 // boost gained per frame while held
#define CYCLE_BOOST_RAMP_DOWN 1	 // boost lost per frame once released
#define CYCLE_GAUGE_MAX 200
#define CYCLE_GAUGE_DRAIN 5		 // per boosted frame: a full gauge lasts 40 frames
#define CYCLE_GAUGE_REFILL 1	 // per frame without boost

typedef struct
{
	world_point position;
	unsigned char heading;
	unsigned char speed; // current speed, ramps up to the cruise speed from a standing start
	unsigned char boost;
	unsigned char bounded; // 1: stopped at the arena edge, 0: free to leave (debug). Fills a padding byte, so free
	short gauge;
} cycle;

// Cruise speed for every cycle, clamped to 1..CYCLE_CRUISE_MAX. Returns the value used.
int cycle_set_cruise_speed(int speed);
int cycle_cruise_speed(void);

void cycle_init(cycle *c, short x, short y, unsigned char heading);
void cycle_turn_left(cycle *c);
void cycle_turn_right(cycle *c);

// Advances one frame. Returns 1 when a limit stopped the cycle: the arena edge when bounded,
// otherwise WORLD_COORD_LIMIT.
int cycle_update(cycle *c, int boosting);

#endif
