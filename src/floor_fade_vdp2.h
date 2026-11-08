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

// The cameras, each seeing the floor differently (view.c)
#define FLOOR_VIEW_FIRST_PERSON 0
#define FLOOR_VIEW_ISO 1
#define FLOOR_VIEW_SPECTATOR 2 // far off and shallow: the arena seen from the stands

// Rebuild the per-row ratios for a camera (FLOOR_VIEW_...)
void floor_fade_set_view(int view);

// The same for the 1st person camera pitched down by tilt (tan x 1000, 0 = level): looking aside
// dips the head (view.c), which moves the horizon up the screen
void floor_fade_set_first_person_tilt(int tilt);

// The same for any camera: a moving one (the opening flight, view.c). tilt: tan(pitch below
// level) x 1000, at most 1732 (60 degrees); focal: (320 / 2) / tan(FOV / 2). Each rebuild works
// out every row with a division, so call it every few frames, not every frame
void floor_fade_set_camera(int tilt, int eye_height, int focal, int fade_start, int fade_end);

// Where the floor's fade begins and where it's full, along the floor, for a view: so the walls
// and trails (object_fog_vdp1.h) fade over the same distances as the floor they stand on.
// Works whether or not the floor fog is on.
void floor_fade_distances(int view, int *fade_start, int *fade_end);

#endif
