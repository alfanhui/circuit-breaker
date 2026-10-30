#ifndef __FLOOR_FADE_VDP2_H__
#define __FLOOR_FADE_VDP2_H__

#include <jo/jo.h>

/*
 * Fades the far floor into black fog: VDP2's line colour screen (black) blended into the floor
 * by a ratio that an HBlank-started DMA changes on every screen row. See floor_fade_vdp2.c.
 * The per-row strengths come from game/floor_fade.c; this file owns the VDP2 side.
 */

// Call once, after the floor plane is set up. Arms the per-row DMA.
void floor_fade_init(void);

// Call from a VBlank callback: points the per-row DMA back at the right row for the next frame.
void floor_fade_vblank(void);

// Rebuild the per-row ratios for the current view (each camera sees the floor differently).
void floor_fade_set_first_person(bool first_person);

// The same for the 1st person camera pitched down by tilt (tan x 1000, 0 = level): looking aside
// dips the head (view.c), which moves the horizon up the screen
void floor_fade_set_first_person_tilt(int tilt);

// Where the floor's fade begins and where it's full, along the floor, for a view: so the walls
// and trails (object_fog_vdp1.h) fade over the same distances as the floor they stand on.
// Works whether or not the floor fog is on.
void floor_fade_distances(bool first_person, int *fade_start, int *fade_end);

#endif
