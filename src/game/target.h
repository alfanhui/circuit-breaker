#ifndef __TARGET_H__
#define __TARGET_H__

#include "world.h"
#include "walker.h"

/*
 * Targeting on foot (play-test 2026-11-09, after Ocarina of Time's Z-targeting, with differences).
 * Pure C, unit-tested on the PC; input.c presses it and view.c turns the head.
 *
 * Six fixed head angles on the face buttons never gave the look you wanted: a Saturn pad has no
 * stick to aim with. So the level says what's worth looking at, and the head locks onto it.
 * Through a level stand markers: a sight (a point of interest: a ledge to reach, a doorway), a
 * cluster (a group of objects worth taking in) and an enemy (the watchdog's drone, later).
 *
 *   X / Z           lock the head onto the target nearest the middle of your view, on its left
 *                   (X) or its right (Z). Pressed again while locked (either): let go
 *   d-pad left/right  while locked, switch to the next target that way round (you don't turn:
 *                   you strafe with L / R, and the d-pad's free)
 *   Y / B           unlocked, look up / down, as before
 *
 * If an enemy's in sight, it's the only thing you can lock onto: X or Z takes it, wherever it is,
 * and switching goes only between enemies.
 *
 * Only the head locks: the body (where you walk and run) stays where it faced, so you can run on
 * past something while you watch it. A target's in reach while it's:
 *   - within TARGET_RANGE of you,
 *   - within TARGET_REACH either side of where your body faces (a head turns about this far, and
 *     the screen shows a little more),
 *   - in sight: no roof in the way that's taller than the lower of you and it (target_in_sight).
 * Out of range or reach, the lock lets go at once (you've run past it). Out of sight, it holds on
 * for TARGET_BLIND_FRAMES first, so a pillar going by doesn't drop it.
 *
 * The maths, on the SH-2 (CLAUDE.md: no division in loops): choosing compares angles worked out
 * once per marker per press (target_bearing: one division each, 16 markers at most, only on a
 * press), sight is cross products only. Each frame, only the locked target's angles are worked out:
 * two divisions a frame, not per marker.
 */

#define TARGETS_MAX 16
#define TARGET_RANGE 2500				 // world units: about 57 m
#define TARGET_REACH 0x4000				 // 90 degrees either side of where you face
#define TARGET_BLIND_FRAMES 30			 // 0.5 s out of sight before the lock lets go
#define TARGET_NONE (-1)

typedef enum
{
	TARGET_SIGHT,	// a point of interest
	TARGET_CLUSTER, // a group of objects
	TARGET_ENEMY,	// takes every lock while it's in sight
} target_kind;

typedef struct
{
	short x, y, z; // where the head aims: world x and y, and the height
	unsigned char kind;
} target_marker;

typedef struct
{
	unsigned char count;
	target_marker markers[TARGETS_MAX];
} target_list;

// Where you look from: your eye (world x, y and height), where your body faces (yaw) and where
// the view looks now (view_yaw: yaw and the head's turn), all ANGLEs 0 .. 0xFFFF
typedef struct
{
	int x, y, z;
	int yaw;
	int view_yaw;
} target_eye;

typedef struct
{
	signed char locked;	  // a marker's index, or TARGET_NONE
	unsigned char blind;  // frames it's been out of sight
} target_lock;

void target_list_clear(target_list *list);
// Adds a marker; returns 0 if the list's full
int target_list_add(target_list *list, int x, int y, int z, target_kind kind);

void target_lock_init(target_lock *lock); // unlocked

// The yaw (walker.h's: 0 faces north, -y; a quarter turn faces east, -x) that faces along (dx, dy)
int target_bearing(int dx, int dy);
// How far up (positive) or down a point dz higher and (dx, dy) away is from you: an ANGLE
int target_pitch(int dz, int dx, int dy);

// Whether nothing in the area stands between the eye and the marker (see the top of the file)
int target_in_sight(const walker_area *area, const target_eye *eye, const target_marker *m);

// X (side -1) or Z (side 1) pressed: locks onto the target nearest the middle of your view on
// that side (or the enemy), or lets go if it's locked. Returns whether it's locked now
int target_press_side(target_lock *lock, const target_list *list, const target_eye *eye, const walker_area *area, int side);
// The d-pad while locked: the next target round to the left (-1) or right (1) of the locked one.
// Stays on it if there's none that way
void target_press_switch(target_lock *lock, const target_list *list, const target_eye *eye, const walker_area *area, int dir);
// Once a frame: lets go of a target out of reach (see the top of the file)
void target_frame(target_lock *lock, const target_list *list, const target_eye *eye, const walker_area *area);

// The head's turn (an ANGLE either way from where the body faces: + right, as view.c's look) and
// pitch (+ up) that look at the locked target. Returns 0 if nothing's locked
int target_aim(const target_lock *lock, const target_list *list, const target_eye *eye, int *head_yaw, int *head_pitch);

#endif
