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
// The prologue's camera: the arena from far off, as a spectator (view.c). It wins over the
// other two; view_init applies a change
extern bool spectating;

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
// Spectating: move the camera dx along x at once, with the cycle (the prologue's loop jumping
// back to its start, input.c), so the jump doesn't show as a pan
void view_spectate_shift(int dx);
// Spectating: move the seat itself (where it pans with and the line it looks across) and the
// camera by (dx, dy), as the loop's break slides the world under it (input.c). The flight down
// starts from the seat (view_arrival_start), so call this first
void view_spectate_move(int dx, int dy);
// Spectating: the line the message is written along (its y). The camera pans along it, staying
// across from it, rather than bobbing after the cycle as it rides up and down the letters
void view_spectate_line(int y);
// Spectating: the point along the message the camera pans with (its x), moving at one steady
// speed (input.c works it out each frame): it looks a little west of it, from far behind
void view_spectate_pan(int x);

// The opening flight into the spectator's seat (game/intro_flight.h), from the cycle where it is
// now. Spectating must be on; view_init applies it, and it lands in the spectator's view by itself
void view_intro_start(const cycle *c);
// Cut it short (the loop broken, or a debug jump): view_init then puts the camera where it should be
void view_intro_stop(void);
bool view_intro_flying(void);
// Its frame, 0..INTRO_FLIGHT_FRAMES (-1: not flying): the debug line shows it (main.c)
int view_intro_frame(void);
// The flight down from the stands into the cycle once the loop's broken (game/arrival_flight.h),
// from the spectator's camera now to the cycle (at its start). Call with spectating still on or
// just turned off, before view_init; play waits for it to land (input.c)
void view_arrival_start(const cycle *c);
void view_arrival_stop(void);
bool view_arriving(void);
int view_arrival_frame(void); // 0..ARRIVAL_FRAMES, -1 when not flying (the debug line)
// Where the camera sees a level plane `height` above the floor: the four points where the rays
// through the screen's corners (an eighth wider all round) meet it, in order round them. Anything
// drawn on that plane inside them is on the screen or just off it, and in front of the camera
// (game/surround.h). Returns false when it can't be seen from above: the camera's under or level
// with it, or spectating
bool view_floor_quad(int height, world_point *quad);
// How far the arena's walls have risen out of the floor: 0..256, 256 (full) but while arriving
int view_wall_rise(void);
// The floor is drawn 2^this times larger (main.c): far off, its tiles grow, so the grid holds up
extern int view_floor_shift;
void view_follow(const cycle *c);
void view_push_world_matrix(void);

#endif
