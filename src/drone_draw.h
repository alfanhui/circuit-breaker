#ifndef __DRONE_DRAW_H__
#define __DRONE_DRAW_H__

#include "game/drone.h"

// Builds the drone's model, once, at start-up
void drone_draw_init(void);

// Before the city's drawn, each frame on foot in the city: the roofs its scan lights, for level.c
void drone_draw_prepare(const drone *d, const drone_box *boxes, int box_count);

/*
 * The watchdog's drone (game/drone.h) where it is, and its scan's sheet (level_scan_draw), its light
 * stopped by the towers (boxes, as for its sight). frame: counts up, for the hover. In the world's
 * matrix, on foot in the city
 */
void drone_draw(const drone *d, const drone_box *boxes, int box_count, unsigned int frame);

#endif
