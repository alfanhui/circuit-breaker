#ifndef __FLOOR_FADE_VDP2_H__
#define __FLOOR_FADE_VDP2_H__

#include <jo/jo.h>

/*
 * Fades the far floor into a horizon haze using VDP2's line colour screen.
 *
 * VDP2 can insert a colour per screen line underneath a layer and mix the two. In
 * additive mode the result is layer + line colour: a black line colour changes nothing,
 * so near rows stay untouched while rows towards the horizon get more haze added.
 * The per-row strengths come from game/floor_fade.c; this file owns the VDP2 side:
 * a palette ramping from black to the haze colour, and the per-line table pointing into it.
 */

// Call once, after the floor and sky planes are set up.
void floor_fade_init(void);

// Rebuild the per-line table for the current view (each camera sees the floor differently).
void floor_fade_set_first_person(bool first_person);

#endif
