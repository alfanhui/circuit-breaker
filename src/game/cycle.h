#ifndef __CYCLE_H__
#define __CYCLE_H__

#include "world.h"
#include "trail.h"

/*
 * Light cycle movement in world space. Pure C, integer only (the SH-2 has no FPU).
 */

#define CYCLE_BASE_SPEED 7		 // default world units per frame at cruise (tunable, see below)
#define CYCLE_CRUISE_MAX 40		 // 2400 units/s: crosses the largest arena in about 12 s
/*
 * Boost (hold Up): extra speed as a share of the cycle's own speed, so it's the same kick at
 * any cruise speed (a fixed +7 was +100% at cruise 7 but only +18% at 40). `boost` is a level,
 * 0..CYCLE_BOOST_FULL, ramping up quickly and fading out more gently; the extra speed is
 *     (speed * CYCLE_BOOST_SHARE * level + 128) >> 8     (>> 8 = / 16 / 16, free on the SH-2;
 *                                                         + 128 rounds to the nearest unit)
 * The gauge drains while boosting. After letting go it waits CYCLE_REFILL_DELAY frames, then
 * refills: so tapping the button can't keep it topped up.
 */
#define CYCLE_BOOST_SHARE 12	 // sixteenths of the speed added at full boost: +75%
#define CYCLE_BOOST_FULL 16		 // the boost level at full
// Felt on the first frame, but built up rather than snapped to: +9% straight away, full in 8
// frames (0.13 s). It was full in 2, which with the FOV kick looked like a glitch
#define CYCLE_BOOST_RAMP_UP 2	 // level gained per frame while held: full in 8 frames
#define CYCLE_BOOST_RAMP_DOWN 1	 // level lost per frame once released: gone in 16 frames (0.27 s)
#define CYCLE_GAUGE_MAX 180
#define CYCLE_GAUGE_DRAIN 2		 // per boosted frame: a full gauge lasts 90 frames (1.5 s)
#define CYCLE_GAUGE_REFILL 1	 // per frame once refilling: empty to full in 180 frames (3 s)
#define CYCLE_REFILL_DELAY 30	 // frames after the last boost before the gauge refills (0.5 s)

typedef struct
{
	world_point position;
	unsigned char heading;
	unsigned char speed; // current speed, ramps up to the cruise speed from a standing start
	unsigned char boost;   // boost level, 0..CYCLE_BOOST_FULL
	unsigned char bounded; // 1: stopped by the arena edge and trails, 0: a ghost (debug). Fills a padding byte, so free
	unsigned char gauge;   // 0..CYCLE_GAUGE_MAX
	unsigned char refill_wait; // frames until the gauge refills. With gauge, fits where a short was
} cycle;

// Cruise speed for every cycle, clamped to 1..CYCLE_CRUISE_MAX. Returns the value used.
int cycle_set_cruise_speed(int speed);
int cycle_cruise_speed(void);

void cycle_init(cycle *c, short x, short y, unsigned char heading);
void cycle_turn_left(cycle *c);
void cycle_turn_right(cycle *c);

// Advances one frame, stopping short of anything in the way (collision.h): the arena edge and
// the given trails when bounded, otherwise only WORLD_COORD_LIMIT. trails may be NULL when
// trail_count is 0. Returns 1 on a crash: the cycle is then stopped (speed and boost 0), and
// builds back up from a standing start once it's turned away.
int cycle_update(cycle *c, int boosting, const trail *const *trails, int trail_count);

#endif
