#ifndef __VIEW_H__
#define __VIEW_H__

#include <jo/jo.h>
#include "game/cycle.h"
#include "game/view_cull.h"
#include "game/wall_clip.h"

// SGL sees the world this many times smaller than world units (see view_push_world_matrix)
#define VIEW_WORLD_SCALE 4

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
// The part of the floor the camera can see out to range, for culling walls and trails.
// free_camera: the debug camera, which can look anywhere.
world_rect view_visible_rect(const cycle *c, bool free_camera, int range);

// The 1st person camera on the floor, for keeping wall corners in front of it.
// Returns false (nothing written) in other views.
bool view_wall_clip_camera(const cycle *c, bool free_camera, wall_clip_camera *out);

// Where the camera stands on the floor and the way it faces (x1024), for fogging walls and
// trails by distance (object_fog_vdp1.h). Returns false for the debug camera: no fog.
bool view_fog_camera(bool free_camera, world_point *eye, int *forward_x, int *forward_y);

// Debug: the old linear turn instead of the ease-out, to compare (turn_ease.h)
extern bool view_linear_turn;

void view_turn_left(void);
void view_turn_right(void);
// 1st person: turn the head to look aside (-1 left, 1 right) or back ahead (0); it eases there
void view_look(int direction);
// Shake the camera: a jolt of this strength (ANGLE) that dies away (game/camera_shake.h).
// In iso it's half the size: the whole arena swinging is too much
void view_shake(int strength);
void view_follow(const cycle *c);
void view_push_world_matrix(void);

#endif
