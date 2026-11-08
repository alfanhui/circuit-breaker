#ifndef __INTRO_FLIGHT_H__
#define __INTRO_FLIGHT_H__

/*
 * The game's opening shot (docs/STORY.md): the camera's flight into the spectator's seat.
 * Pure C, unit-tested on the PC; view.c flies the camera with it.
 *
 * It starts face down to the floor, over the cycle as it begins writing "SEGA SATURN SH-2".
 * Then it lifts its head, slowly picks up speed and rides along beside the cycle as it draws,
 * and banks round, climbing, into the spectator's view.
 *
 * The flight is a handful of keyframes, each a whole camera: where it is, which way it looks,
 * its roll and its FOV. Between them it follows a Catmull-Rom spline: a curve through every key
 * that carries its speed through each one, so the camera never stops at a key or turns a sharp
 * corner (easing key to key would stop it at each). Each value runs between two keys on
 *
 *     p(t) = ( 2 p1 + (p2 - p0) t + (2 p0 - 5 p1 + 4 p2 - p3) t^2 + (3 p1 - p0 - 3 p2 + p3) t^3 ) / 2
 *
 * for t from 0 (at key p1) to 1 (at key p2), p0 and p3 being the keys either side. Before the
 * first key and after the last it pretends the neighbouring key is there again, which makes it
 * start from rest and settle to rest (the curve's slope at a key is (p2 - p0) / 2: 0 there).
 *
 * Keys are INTRO_SEGMENT_FRAMES apart, a power of two, so t is a shift of the frame count
 * with no division, and t^2 and t^3 are a multiply and a shift each (Horner's rule).
 *
 * Positions are from the point the camera follows (view.c): at first the cycle's start, held
 * still; as `follow` rises (0 to 256) it slides over to a point easing after the cycle, `lead`
 * ahead of it in x, on the message's centre line. That's how it starts still and picks up speed.
 * The last key is exactly the spectator camera (view.c's SPECTATE_*), so it lands with no jump.
 */

#define INTRO_SEGMENT_SHIFT 7 // 128 frames, ~2.1 s, between keys
#define INTRO_SEGMENT_FRAMES (1 << INTRO_SEGMENT_SHIFT)
#define INTRO_FLIGHT_KEYS 5
#define INTRO_FLIGHT_FRAMES ((INTRO_FLIGHT_KEYS - 1) * INTRO_SEGMENT_FRAMES) // 512: 8.5 s

// Degrees to SGL's ANGLE (65536 a circle), in whole numbers: the same as SGL's DEGtoANG gives
#define INTRO_DEG(d) ((d) * 65536 / 360)
#define INTRO_FOLLOW_FULL 256

typedef struct
{
	int ox;			// the camera from the point it follows, along x (west +) and y (south +)
	int oy;
	int height;		// above the floor
	int rx;			// pitch, ANGLE: 0 looks straight down, INTRO_DEG(90) level
	int rz;			// turn, ANGLE: 0 faces north, + turns right (east)
	int roll;		// lean, ANGLE: banking through a turn
	int fov;		// field of view, ANGLE
	int lead;		// how far west (+x) of the cycle the followed point runs
	int follow;		// 0: held at the cycle's start; INTRO_FOLLOW_FULL: following the cycle
	int fade_start; // the floor's fog: where it begins and where it's full (floor_fade.h)
	int fade_end;
} intro_shot;

// The camera at a frame of the flight (0..INTRO_FLIGHT_FRAMES; outside that, the nearest end)
void intro_flight_at(int frame, intro_shot *shot);

// Any flight through count keys, segment_frames apart (game/arrival_flight.c flies one too), at a
// frame from 0 to (count - 1) x segment_frames. t_step is 65536 / segment_frames, a constant the
// compiler works out, so t is a multiply and a shift. The slopes are kept from overshooting
// (intro_flight.c's tangent): the camera never swings past a key and back
void flight_at(const intro_shot *keys, int count, int segment_frames, int t_step, int frame, intro_shot *shot);
// A key's name ("floor", "alongside"...), for the debug line. The segment playing at a frame is
// frame >> INTRO_SEGMENT_SHIFT: from that key to the next
const char *intro_flight_key_name(int key);

#endif
