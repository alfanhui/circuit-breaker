#ifndef __DRONE_DRAW_H__
#define __DRONE_DRAW_H__

#include "game/drone.h"

// Builds the drone's model, once, at start-up
void drone_draw_init(void);

// Before the city's drawn, each frame on foot in the city: the roofs the nearest drone's scan line
// lights (of `count`), for level.c. Only the nearest's: each costs a walk over the boxes
void drone_draw_prepare(const drone *drones, int count, const drone_box *boxes, int box_count);

/*
 * A watchdog's drone (game/drone.h) where it is, and its beam (level_beam_draw). frame: counts up,
 * for the hover. In the world's matrix, on foot in the city
 */
void drone_draw(const drone *d, unsigned int frame);

#endif
