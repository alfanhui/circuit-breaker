#ifndef __DRONE_H__
#define __DRONE_H__

#include <stdbool.h>

/*
 * The watchdog's drone (docs/WORK_RAM.md, "The watchdog drone" and "The watchdog in Low Work
 * RAM"). Pure C, unit-tested on the PC (tests/test_drone.c); input.c flies it, drone_draw.c draws
 * it. Seen, the beam turns red, the screen glows (game/seen_glow.h) and it chases you; lost, it
 * searches; kept in its beam, you're caught (its moods: DRONE_PATROL, below).
 *
 * Its patrol is a route of stops, each a place in the air (world x, y, z), a heading its sensor
 * looks to while it's there, and how long it waits. A route is a loop (after the last stop, round
 * to the first) or back and forth (at either end, it turns back).
 *
 * It flies, rather than sliding from stop to stop: it steers its yaw towards the next stop, at most
 * DRONE_TURN_MAX a frame, and moves along where it faces. So it banks round a corner, and slows to
 * turn on the spot when its stop is behind it. Its height eases to the stop's on its own.
 *
 * A patrol a player can learn, but not to the frame (the user, 2026-11-17: "a bit of randomness,
 * both in stopping, turn a different way, its movement not always straightforward but stagger").
 * Each drone has its own seeded random numbers (drone_random), so a film plays the same every run:
 *   - each stop's wait is 0.5 to 1.5 times the route's;
 *   - waiting, it may glance a quarter turn aside, then back;
 *   - leaving a stop, it may turn back the other way round its route;
 *   - each leg's speed differs (DRONE_SPEED_MIN to + DRONE_SPEED_SPREAD);
 *   - it weaves either side of its line (DRONE_SWAY);
 *   - now and then, in flight, it hesitates.
 *
 * Its beam is a square pyramid (the user: "a square cone scan effect ... so we can always see
 * it"). Its apex is the sensor, under its nose. Its base is a square on the roofs ahead,
 * DRONE_BEAM_AHEAD in front of the sensor and DRONE_BEAM_HALF either way, at the height of the roof
 * under the square's middle (beam_z, eased so it doesn't jump). You're seen inside the pyramid
 * (each of its four sides' planes has you on its inner side), with no tower between the sensor
 * and your eye. The same planes say which sides to draw: those you're outside of face you, so from
 * anywhere outside the beam at least one side shows, and from inside none (the glow tells you).
 *
 * The scan line, a CRT's, still sweeps across the square's roofs (lighting them: drone_scan_lit),
 * from its near edge to its far edge in DRONE_SCAN_FRAMES, then flies back. It's only for show now:
 * the pyramid is what sees.
 *
 * World units (a unit ~2.3 cm: walker.h). x and y as the world's (world.h: east is -x, north is
 * -y). Angles as SGL's: 0x10000 a whole turn, heading h at h << 14
 */

#define DRONE_STOPS_MAX 8
#define DRONE_SPEED_MIN 640	   // a leg's speed, x256 a frame: 2.5 (walking's 3) ...
#define DRONE_SPEED_SPREAD 512 // ... up to this much more (4.5)
#define DRONE_CLIMB 3		   // up or down, a frame: a lift's pace
#define DRONE_TURN_MAX 0x180   // flying, the most it turns a frame: ~2 degrees, a quarter turn in 0.7 s
#define DRONE_TURN_SHIFT 3	   // waiting, the turn eases: an eighth of what's left of it a frame
#define DRONE_ARRIVE 24		   // within this of a stop (each way), it's there
#define DRONE_SWAY 96		   // the weave: aside at this, x256 a frame, ...
#define DRONE_SWAY_SHIFT 6	   // ... flipping every 64 frames: about 12 either side of its line
#define DRONE_SWAY_NEAR 300	   // and none this near a stop, so it arrives
#define DRONE_NOSE 140		   // its sensor, ahead of its middle (drone_draw.c's model: its nose)
#define DRONE_SENSOR_DOWN 10   // and under it

// The beam's square on the roofs: 350 to 1050 ahead of the sensor, 700 across (a lane's 500)
#define DRONE_BEAM_AHEAD 700
#define DRONE_BEAM_HALF 350
#define DRONE_BEAM_DROP 500 // its square this far under the sensor, until it finds a roof
#define DRONE_BEAM_REACH 2048 // nothing further than this from the sensor (each way) is in it
#define DRONE_SETTLE_EVERY 8  // frames between looks for the roof under the beam (a power of two)

/*
 * Its moods (the user, 2026-11-17: "the player must run and hide, or escape via elevator. The watch
 * dog needs to chase the player, but is only a bit slower than player ... then search around when
 * player is hidden"):
 *   DRONE_PATROL  its route (and, returning, back to it)
 *   DRONE_CHASE   it's seen you: after you, over the roofs, a little slower than your run
 *   DRONE_SEARCH  it's lost you: it looks round where it last saw you, then goes back
 * Seen for DRONE_CATCH_FRAMES (the count goes up a frame in its beam, down a frame out of it) and
 * you're caught: input.c resets you to your last checkpoint
 */
#define DRONE_PATROL 0
#define DRONE_CHASE 1
#define DRONE_SEARCH 2
#define DRONE_CHASE_SPEED 1280 // x256 a frame: 5 (your run's 6)
#define DRONE_CHASE_TURN 0x300 // twice its patrol's: a quarter turn in a third of a second
#define DRONE_CHASE_CLIMB 6
#define DRONE_CHASE_ABOVE 350 // chasing, this high over where it saw your eye
#define DRONE_STANDOFF (DRONE_NOSE + DRONE_BEAM_AHEAD) // seeing you, it holds back this far: you in its square's middle
#define DRONE_LOSE_FRAMES 20	 // unseen this long, it goes to where it saw you last
// 2.6 s in its beam (not counting the time out of it) and you're caught. Was 2 s: 30% more, to give
// you time to get away (the user, 2026-11-17: "Increase the watchdog detection duration by 30%")
#define DRONE_CATCH_FRAMES 156
#define DRONE_SEARCH_FRAMES 480	 // 8 s looking round, then back to its patrol
#define DRONE_SEARCH_SPEED 768	 // 3 a frame
#define DRONE_SEARCH_REACH 500	 // its hops: up to this far (each way) from where it lost you
#define DRONE_SEARCH_THERE 40	 // within this of a point, it's there
#define DRONE_CLEAR 150			 // off its patrol it keeps this over ...
#define DRONE_CLEAR_AROUND 400	 // ... the tallest tower this near it (each way)

#define DRONE_SCAN_SHIFT 7 // the scan line crosses the square in 128 frames (~2 s), then flies back
#define DRONE_SCAN_FRAMES (1 << DRONE_SCAN_SHIFT)
#define DRONE_SCAN_DEEP 60 // the line's depth, along where it faces
#define DRONE_SCAN_SETTLED 0x0800 // the line's drawn only while it faces within ~11 degrees of an axis
#define DRONE_SIGHT_STEPS_SHIFT 4 // a line of sight is tested at 16 points (each ~100 apart at most)
#define DRONE_LIT_MAX 12		  // the most roofs the line lights at once

typedef struct
{
	short x, y, z;		   // where it stops, in the air
	unsigned char heading; // where its sensor looks while it's there: HEADING_NORTH ... (world.h)
	unsigned short wait;   // frames it waits there, before the randomness
} drone_stop;

// A tower, as the sight test sees it: a box from the ground up to its roof
typedef struct
{
	short x_min, x_max, y_min, y_max;
	short top;
} drone_box;

typedef struct
{
	int x, y, z;
} drone_point;

// The beam's pyramid: the sensor, and its base's corners round the square (near left, near right,
// far right, far left: side k runs from base[k] to base[k + 1], the last back to base[0])
typedef struct
{
	drone_point apex;
	drone_point base[4];
} drone_beam;

typedef struct
{
	const drone_stop *route;
	unsigned char stops;		 // in the route
	unsigned char at;			 // the stop it's at, or flying to
	signed char step;			 // +1 or -1: which way along the route it's going
	bool back_and_forth;		 // at the route's ends it turns back (or it's a loop)
	bool waiting;				 // at its stop (or flying to it)
	signed char glance;			 // this wait's glance aside: -1, +1, or 0 for none
	unsigned short waited, wait; // frames it's waited at this stop, and how long it will
	unsigned short speed;		 // this leg's, x256 a frame
	unsigned char pause;		 // frames left of a hesitation in flight
	unsigned char sway;			 // the weave's phase: frames into it
	int sub_x, sub_y;			 // where it is, x256 (x and y below are these, whole)
	short x, y, z;
	unsigned short yaw;	 // how it's turned (0 .. 0xFFFF)
	short beam_z;		 // the height of its beam's square: the roof's under it, eased
	unsigned char scan;	 // the scan line: frames into its sweep, 0 .. DRONE_SCAN_FRAMES - 1
	unsigned char settle_phase; // which of each DRONE_SETTLE_EVERY frames it looks for the roof
	unsigned int random; // its random numbers' state
	bool seen;			 // its beam has you this frame (drone_notice)
	unsigned char mode;	 // DRONE_PATROL, DRONE_CHASE or DRONE_SEARCH
	bool returning;		 // on patrol, but on its way back to it (kept clear of the towers)
	unsigned char unseen; // frames since it last saw you (up to 255)
	unsigned char held;	  // the catch: up a frame seen, down a frame not (caught at DRONE_CATCH_FRAMES)
	short lost_x, lost_y, lost_z; // where it last saw your eye
	short search_x, search_y;	  // searching: the point it's hopping to
	unsigned short look;		  // searching: the way it turns to look, hovering
	unsigned short search_left;	  // searching: frames left
	short cruise_z;				  // clear of the towers round it (look_around)
} drone;

// At the route's first stop, waiting there, its random numbers from `seed`. route: lasts (a
// constant, or static)
void drone_init(drone *d, const drone_stop *route, int stops, bool back_and_forth, unsigned int seed);

// Its next random number, 0 .. 255
int drone_random(drone *d);

/*
 * One frame: it waits (and glances), or flies on to its stop; the scan line sweeps on; its beam's
 * square eases to the height of the roof under it (the boxes: where the roofs are)
 */
void drone_update(drone *d, const drone_box *boxes, int box_count);

/*
 * After drone_sees, once a frame: whether it sees your eye at (x, y, z) now. Seen, it chases (from
 * any mood) and the catch counts up; not, the catch counts down, and a chase that's lost you goes
 * to search (drone_update)
 */
void drone_notice(drone *d, bool seen, int x, int y, int z);

// It's had you in its beam long enough: you're caught
bool drone_caught(const drone *d);

// Where it faces now, as a unit vector x1024 (world x and y)
void drone_forward(const drone *d, int *fx, int *fy);

// Where its sensor is: under its nose
void drone_sensor(const drone *d, int *x, int *y, int *z);

// Its beam's pyramid now
void drone_beam_get(const drone *d, drone_beam *b);

/*
 * Which of the beam's sides (x, y, z) is outside of: bit k for side k. 0 is inside the beam; any
 * other is outside, and those sides face (x, y, z), so they're the ones to draw seen from there.
 * Past DRONE_BEAM_REACH from the apex (either way) it's 0xF: outside, and all would face it
 */
unsigned drone_beam_outside(const drone_beam *b, int x, int y, int z);

// Whether the beam sees an eye at (x, y, z): inside it, and none of the boxes between the sensor
// and it. Once a frame a drone (16 points along the line of sight, against the boxes it touches)
bool drone_sees(const drone *d, int x, int y, int z, const drone_box *boxes, int box_count);

// The axis it faces (HEADING_NORTH ...), or -1 if it isn't within DRONE_SCAN_SETTLED of one
int drone_settled_heading(const drone *d);

// How far ahead of the sensor the scan line's near edge is now (the square's near edge to its far)
int drone_scan_ahead(const drone *d);

// The scan line on the roofs now, as a rectangle in the world (top: unused), along `heading`
void drone_scan_rect(const drone *d, int heading, drone_box *rect);

/*
 * The roofs the scan line lights now: each box it crosses, the part of its roof under the line
 * (clipped to it, top its roof's height), if the sensor can see that part's middle. None unless
 * it's settled on an axis (drone_settled_heading). Writes up to DRONE_LIT_MAX, returns how many
 */
int drone_scan_lit(const drone *d, const drone_box *boxes, int box_count, drone_box *lit);

#endif
