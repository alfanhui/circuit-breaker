#ifndef __FLOOR_FADE_H__
#define __FLOOR_FADE_H__

/*
 * How strongly to fade each screen row of the floor into the horizon haze.
 * Pure C, unit-tested on the PC; floor_fade_vdp2.c turns the levels into VDP2 line colours.
 *
 * A camera that doesn't roll sees the floor at one distance per screen row. For a camera
 * pitched down by angle a, a row dy pixels below the screen centre looks down at a + atan(dy / focal),
 * so (with tan(a + b) expanded) the floor it shows is
 *     distance = eye_height * (focal - tan(a) * dy) / (tan(a) * focal + dy)
 * For a level camera (a = 0) that's just eye_height * focal / dy: similar triangles.
 * tan(a) is stored x1000 so it all stays integer maths.
 *
 * Far rows bunch up towards the horizon, which is where the floor's grid lines shimmer or
 * merge into moire; fading them hides that and adds depth. Rows above the horizon (sky) get a
 * short glow that matches, so the two blend.
 */

#define FLOOR_FADE_LEVELS 16 // 0 = untouched, FLOOR_FADE_LEVELS - 1 = full haze

typedef struct
{
	int centre_row; // screen row of the projection centre
	int tilt;		// tan(downward pitch) x 1000: 0 for a level camera
	int eye_height; // world units above the floor
	int focal;		// distance to the screen in pixels: (width / 2) / tan(horizontal fov / 2)
	int fade_start; // floor distance where the fade begins
	int fade_end;	// floor distance where it's full
	int glow_rows;	// rows of glow above the horizon
} floor_fade_view;

// Writes one level per screen row (0..rows - 1) into levels.
void floor_fade_rows(const floor_fade_view *view, unsigned char *levels, int rows);

// VDP2's colour calculation ratio for a level: how much of the fog colour is mixed into the floor,
// in 32nds (0 = none, FLOOR_FADE_RATIO_MAX = all but 1/32 of the floor). See floor_fade_vdp2.c.
#define FLOOR_FADE_RATIO_MAX 31
int floor_fade_ratio(int level);

#endif
