#ifndef __ARRIVAL_FLIGHT_H__
#define __ARRIVAL_FLIGHT_H__

#include "intro_flight.h"
#include "world.h"

/*
 * The camera's flight from the stands down into the cycle, once the loop's broken (docs/STORY.md).
 * Pure C, unit-tested on the PC; view.c flies the camera with it, input.c holds the game still
 * meanwhile.
 *
 * At the third press the world slides so the loop's cycle is at the arena's centre (within half
 * a floor tile, input.c), and it turns north: the flight is always the same one. Then in 3 s the camera climbs out of the spectator's seat to an overview
 * of the whole arena while its walls rise out of the floor (so you see the box you're in), and
 * swoops down behind the cycle into the 1st person view.
 *
 * The same spline as the opening flight (intro_flight.h's flight_at), through ARRIVAL_KEYS keys
 * ARRIVAL_SEGMENT_FRAMES apart. The first key is wherever the spectator's camera was; the rest
 * are placed from the cycle's start (ox, oy from it, along x and y; lead and follow unused). The
 * last is exactly the 1st person camera, so it lands with no jump.
 */

#define ARRIVAL_KEYS 5
#define ARRIVAL_SEGMENT_FRAMES 45 // 0.75 s between keys: not a power of two, so t is x 65536 / 45
#define ARRIVAL_FRAMES ((ARRIVAL_KEYS - 1) * ARRIVAL_SEGMENT_FRAMES) // 180: 3 s

// The walls rise over 2^ARRIVAL_RISE_SHIFT frames from ARRIVAL_RISE_FROM: up as the overview shows them
#define ARRIVAL_RISE_FROM 10
#define ARRIVAL_RISE_SHIFT 6 // 64 frames, 1.1 s
#define ARRIVAL_RISE_FULL 256

// Below this the camera is among the walls: view.c then keeps their corners in front of it, as
// it does in 1st person (wall_clip.h). Above it, it sees them all from above. A unit test checks
// the flight is well inside the arena whenever it's this low
#define ARRIVAL_LOW_HEIGHT 2000

// The camera at a frame of the flight (0..ARRIVAL_FRAMES), from the spectator's camera (from:
// its whole place, x and y in the world) down to the cycle at start. x and y come out in the world
void arrival_flight_at(const intro_shot *from, world_point start, int frame, intro_shot *shot);

// How far the arena's walls have risen at a frame: 0 (flat in the floor) to ARRIVAL_RISE_FULL
int arrival_wall_rise(int frame);

// A key's name ("stands", "overview"...), for the debug line
const char *arrival_key_name(int key);

#endif
