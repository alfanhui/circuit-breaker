#ifndef __DRONE_H__
#define __DRONE_H__

#include <stdbool.h>

/*
 * The watchdog's drone (docs/WORK_RAM.md, "The watchdog drone"): build step 4, its route and its
 * sight. Pure C, unit-tested on the PC (tests/test_drone.c); input.c moves it, drone_draw.c draws
 * it. Noise, suspicion and the reset (step 5) come next: for now being seen only shows (the scan
 * turns red).
 *
 * It rides the towers' faces up and down between memory slots (floors), like a lift: a route of
 * stops, each a place in the air (world x, y, z), a heading its sensor faces while it's there, and
 * how long it waits. Between stops it flies straight at DRONE_SPEED a frame, each axis on its own
 * (no division: a step of at most DRONE_SPEED along each), and turns to the next stop's heading as
 * it goes. After the last stop, round to the first.
 *
 * Its scan is a beam of light angled down from its sensor (under its nose, DRONE_NOSE ahead) to a
 * line on the roofs ahead of it: the scan line. Like a CRT's, the line sweeps out across the
 * ground, from DRONE_SCAN_NEAR ahead to DRONE_SCAN_RANGE, in DRONE_SCAN_FRAMES, then flies back.
 * It's DRONE_SCAN_DEEP deep and as wide as the beam's fan (45 degrees either side: at a distance f
 * ahead, f either side), a rectangle on the ground (it faces along an axis). The roofs it lights are
 * coloured (drone_scan_lit), but not one a tower stands between the sensor and: its shadow. You're
 * seen when the line's under you and nothing's between its sensor and your eye. So you hide behind
 * a tower (in its shadow), out of its fan, or out of its reach.
 * (It was first a level sheet sweeping down through one floor's height, its sensor at your
 * height: the user, 2026-11-14, asked for it higher and its scan angled down, lighting the roofs)
 *
 * World units (a unit ~2.3 cm: walker.h). x and y as the world's (world.h: east is -x, north is -y)
 */

#define DRONE_STOPS_MAX 8
#define DRONE_SPEED 6			  // a frame, along each axis: ~8 m/s, a lift's pace and a bit
#define DRONE_TURN_SHIFT 3		  // the turn eases: an eighth of what's left of it a frame
#define DRONE_NOSE 140			  // its sensor, ahead of its middle (drone_draw.c's model: its nose)
#define DRONE_SENSOR_DOWN 10	  // and under it
#define DRONE_SCAN_NEAR 200		  // the scan line sweeps out from this far ahead ...
#define DRONE_SCAN_RANGE 1400	  // ... to this far (~32 m, three towers)
#define DRONE_SCAN_SHIFT 7		  // in 128 frames (~2 s), then flies back
#define DRONE_SCAN_FRAMES (1 << DRONE_SCAN_SHIFT)
#define DRONE_SCAN_DEEP 60		  // the line's depth, along where it faces: 10 a frame, so it's
								  // over any point it passes for 6 frames
#define DRONE_SIGHT_STEPS_SHIFT 4 // a line of sight is tested at 16 points (each ~100 apart at most)
#define DRONE_LIT_MAX 12		  // the most roofs the line lights at once (it spans ~5 towers)

typedef struct
{
	short x, y, z;		   // where it stops, in the air
	unsigned char heading; // where its sensor faces there: HEADING_NORTH ... (world.h)
	unsigned short wait;   // frames it waits there
} drone_stop;

// A tower, as the sight test sees it: a box from the ground up to its roof
typedef struct
{
	short x_min, x_max, y_min, y_max;
	short top;
} drone_box;

typedef struct
{
	const drone_stop *route;
	unsigned char stops; // in the route
	unsigned char at;	 // the stop it's at, or flying to
	unsigned short waited;
	short x, y, z;
	unsigned short yaw; // how it's turned now (0 .. 0xFFFF; heading h is h << 14), easing to the stop's
	unsigned char scan; // the sweep: frames into it, 0 .. DRONE_SCAN_FRAMES - 1
	bool seen;			// its scan has you this frame (input.c sets it from drone_sees)
} drone;

// At the route's first stop, waiting there. route: lasts (a constant, or static)
void drone_init(drone *d, const drone_stop *route, int stops);

// One frame: it waits, or flies on to the next stop, and turns; the scan sweeps on
void drone_update(drone *d);

// Whether it's stopped at a stop (not flying between)
bool drone_waiting(const drone *d);

// The heading it faces (or is turning to): the stop's it's at or flying to
int drone_heading(const drone *d);

// Where its sensor is: under its nose
void drone_sensor(const drone *d, int *x, int *y, int *z);

// How far ahead the scan line's near edge is now (DRONE_SCAN_NEAR .. DRONE_SCAN_RANGE)
int drone_scan_ahead(const drone *d);

// The scan line on the ground now, as a rectangle in the world (its top: unused)
void drone_scan_rect(const drone *d, drone_box *rect);

/*
 * Whether the scan sees an eye at (x, y, z): the line's under it, and none of the boxes is between
 * the sensor and it. Once a frame (16 points along the line of sight, each tested against the
 * boxes the line's bounds touch)
 */
bool drone_sees(const drone *d, int x, int y, int z, const drone_box *boxes, int box_count);

/*
 * The roofs the scan line lights now: each box it crosses, the part of its roof under the line
 * (x and y clipped to it, top its roof's height), if the sensor can see that part's middle (no
 * other box between). Writes up to DRONE_LIT_MAX, returns how many
 */
int drone_scan_lit(const drone *d, const drone_box *boxes, int box_count, drone_box *lit);

// How high the ground is under the scan line's middle: the roof there, or `none` over a street
int drone_scan_ground(const drone *d, const drone_box *boxes, int box_count, int none);

#endif
