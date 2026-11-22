#ifndef __SKY_SLIDE_H__
#define __SKY_SLIDE_H__

/*
 * The sky slid with you (the user, 2026-11-16: "when in the arena, and a new iteration begins, the
 * landmarks jump a bit as the player is moved as the new arena walls are raised").
 *
 * As a round's layout rises, you're slid to its start in one frame, by whole floor tiles (input.c,
 * between_rounds: ROUND_RISES), so the grid under you lands on itself and nothing near you seems to
 * move. But the sky's furniture (the landmarks, the CD block's disc, the skyline ring round the
 * arena) stays where it is in the world, so on that frame it all jumped by the slide. The same as
 * the attract loop's cycle is slid to the arena's middle (slide_to_the_centre).
 *
 * So the sky is slid with you: the slide is kept here, and the sky's drawn that far over (all of it
 * for the landmarks and the disc; for the skyline ring, which already follows you three quarters
 * of the way, the quarter it doesn't). On the frame you're moved, nothing in the sky moves. Then the
 * slide eases back to nothing, SKY_SLIDE_EASE units a frame each way: the landmarks drift back to
 * where they stand, about a degree a second at their distance, under everything else moving as you
 * ride, and are back long before the corridor opens.
 *
 * The sky's things are placed in the corridor's frame (corridor_point_behind: `out` from the
 * arena's middle along the side's axis, `across` along the other), so the slide is handed to them
 * in that frame (sky_slide_frame). Pure C, no jo: tests/test_sky_slide.c
 */

#define SKY_SLIDE_EASE 6 // a frame, each way: 8000 (half the arena) back in ~22 s

typedef struct
{
	int x, y; // in the world: how far the sky's drawn from where it stands
} sky_slide;

void sky_slide_init(sky_slide *s);

// You were moved by (dx, dy): the sky goes with you
void sky_slide_add(sky_slide *s, int dx, int dy);

// Once a frame: a step back towards where everything stands
void sky_slide_step(sky_slide *s);

// The slide in the corridor's frame, for a corridor behind `side`'s wall: added to `out` (or the
// wall's distance: corridor_point_behind adds the two) and to `across`, it moves a point by the slide
void sky_slide_frame(const sky_slide *s, int side, int *out, int *across);

#endif
