#ifndef __VIEW_H__
#define __VIEW_H__

#include <jo/jo.h>
#include "game/cycle.h"

/*
 * Camera for both views. Everything in the world is drawn inside view_push_world_matrix(),
 * so objects only translate by their own world position.
 */

extern bool first_person;

// Camera focus in world space and the floor height relative to the camera (old pos.x/y/z)
extern int view_x;
extern int view_y;
extern int view_z;

// World rotation (SGL ANGLE units, 0x4000 = 90 degrees)
extern ANGLE view_rx;
extern ANGLE view_rz;

void view_init(const cycle *c);
void view_toggle(const cycle *c);
void view_turn_left(void);
void view_turn_right(void);
void view_follow(const cycle *c);
void view_push_world_matrix(void);

#endif
