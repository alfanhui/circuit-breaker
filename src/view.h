#ifndef __VIEW_H__
#define __VIEW_H__

#include <jo/jo.h>
#include "game/cycle.h"
#include "game/view_cull.h"
#include "game/wall_clip.h"
#include "game/frustum.h"

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

// World rotation (SGL jo_angle units, 0x4000 = 90 degrees)
extern jo_angle view_rx;
extern jo_angle view_rz;

void view_init(const cycle *c);
void view_toggle(const cycle *c);
// The part of the floor the camera can see out to range, for culling walls and trails.
// free_camera: the debug camera, which can look anywhere.
world_rect view_visible_rect(const cycle *c, bool free_camera, int range);

// In 1st person, the camera's planes in 3D (game/frustum.h): near, and the screen's four edges a
// sixteenth wider than the screen. The corridor and the city are cut to them (level.c).
// Returns false (nothing written) in other views, or with the debug camera
bool view_frustum(bool free_camera, frustum *out);
// The same on the flight down into the cycle (false at any other time): view_frustum has none
// then, as nothing that's cut to the view is in play yet. But the sky's furniture is drawn through
// the flight (level_sky_draw), so the skyline and the landmarks are there from the start of the
// descent, not popped in as you land
bool view_arrival_frustum(frustum *out);

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
// On foot (game/walker.h), every frame: the body facing yaw (jo_angle, any way at all, at once), the
// head looking look_x (-1 left, 1 right) and look_y (-1 down, 1 up), easing there, and the eye bob
// lower than standing on the floor (a step's bob, a landing's dip, less the roof's height). Off
// again at view_init
void view_on_foot(int yaw, int look_x, int look_y, int bob);
// On foot, after view_on_foot: the head locked onto a target (game/target.h) instead of the
// buttons' look: its turn from where the body faces (+ right) and its pitch (+ up), ANGLEs. Eased
// there as a look is, and kept within how far a head turns (LOOK_ANGLE, LOOK_UP_ANGLE, LOOK_DOWN_ANGLE)
void view_head_aim(int head_yaw, int head_pitch);
// On foot, how far the head's turned to the side now (+ right): where the view looks is the
// body's yaw and this
int view_look_yaw(void);
// Where a point the head's aiming at (view_head_aim's angles) is on the screen now, in pixels
// from its centre (+y down): the head eases there, so until it's settled it's off the middle.
// False if it's off the screen
bool view_aim_on_screen(int head_yaw, int head_pitch, int *screen_x, int *screen_y);
// Whether you're on foot (view_on_foot since the last view_init)
bool view_is_on_foot(void);
// On foot, how far the head's looking down now (jo_angle; negative: up). 0 otherwise
int view_look_pitch(void);
// How high the floor's grid (VDP2) lies: 0 riding, the arena's floor; on foot, far below, as the
// ground under Work RAM's city (city.h: CITY_GROUND), the towers' feet
int view_ground_height(void);
// Shake the camera: a jolt of this strength (jo_angle) that dies away (game/camera_shake.h).
// In iso it's half the size: the whole arena swinging is too much
void view_shake(int strength);
// Keep the camera rumbling at least this hard (jo_angle), called every frame while it should
// (camera_shake_hold): it dies away once the calls stop
void view_rumble(int strength);
// The designed crash (game/breach.h): the camera's roll and, in 1st person, its tip down towards
// the floor (jo_angle) and how far the eye's dropped, each frame of it. Back to 0 when the game
// starts again (view_init)
void view_breach(int roll, int dip, int drop);
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
// The same for the floor itself, however low the camera (1st person's eye is only 35 up, too low
// for view_floor_quad): for things laid on the floor (the corridor's glass, level.c)
bool view_ground_quad(world_point *quad);
// The same for a roof at a height (level.c's grey box): seen however little the eye's above it,
// as you stand on it. False when the eye's not above it
bool view_roof_quad(int height, world_point *quad);

// How far to move the sky (main.c) up or down for the camera's pitch now: the sky's drawn
// outside the world's matrix, so it doesn't tilt with the camera on its own. 0 looking level
int view_sky_rise(void);
// How far the arena's walls have risen out of the floor: 0..256, 256 (full) but while arriving
int view_wall_rise(void);
// The floor is drawn 2^this times larger (main.c): far off, its tiles grow, so the grid holds up
extern int view_floor_shift;
void view_follow(const cycle *c);
void view_push_world_matrix(void);

#endif
