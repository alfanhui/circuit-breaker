#ifndef __JUMPS_H__
#define __JUMPS_H__

#include <stdbool.h>
#include "corridor.h"

/*
 * The pause menu's "Jump to" (game/settings.h; the user, 2026-11-17: "Please add location jump to
 * debug menu. Main locations so that we can test faster"). Each is a place on foot to stand, and
 * which way to face: in the corridor, by where it's open (its out and across), or on a city tower
 * (its column and row). Pure C, unit-tested on the PC (tests/test_jumps.c: each one stands you on
 * something); input.c puts you there.
 *
 * The first, JUMP_NONE, is "stay": R goes round the list, a jump at each press, and back round to
 * it, which moves you nowhere
 */
typedef enum
{
	JUMP_NONE,
	JUMP_START,	  // where you get up, after the crash
	JUMP_YARD,	  // the corridor's right half, before its rail: the moves to try
	JUMP_STAIRS,  // the left half, past the crate, at the foot of the stairs
	JUMP_ENTRY,	  // the city's entry roof, through the doorway
	JUMP_DRONE_1, // inside the first watchdog's loop (0B00: watchdogs.steps' place)
	JUMP_DRONE_2, // beside the second's back and forth (0C30), facing along it
	JUMP_CD_HUB,  // the CD block's hub, short of its lift's pad
	JUMP_BACKUP,  // Backup RAM's hub
	JUMP_SCSP,	  // the sound's hub (the SCSP)
	JUMP_COUNT
} jump_place;

#define JUMP_NAME_MAX 9 // the longest name's characters: so the menu's line fits (settings.h)

// The place's name on the menu
const char *jump_name(int place);

/*
 * Where the place is for the corridor `c` (open behind its wall): the point to stand on, the
 * height of what's under it (a roof's: walker_height) and the way to face (a binary angle, as
 * walker's yaw). JUMP_NONE has none: false, and nothing's written
 */
bool jump_where(const corridor *c, int place, world_point *at, int *height, int *yaw);

#endif
