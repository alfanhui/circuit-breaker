#ifndef __HUD_H__
#define __HUD_H__

#include <jo/jo.h>
#include "game/cycle.h"

/*
 * On-screen readouts for the player (the debug text is separate: main.c).
 * One instrument at the bottom centre of the screen: the radar, with the boost gauge as its rim
 * and markers pointing at the bots, in both views. It turns with the camera: in 1st person with
 * the cycle, in iso it's fixed to the iso camera's 45 degree turn, so up on the radar is always
 * up the screen.
 */

// Makes the HUD's textures. Once, at start-up, after the other textures are loaded.
void hud_init(void);

// Every frame, after the world is drawn: the gauge, round the radar's rim
void hud_draw_boost(const cycle *c);

// A rider to point at from the radar's rim, however far away
typedef struct
{
	world_point position;
	unsigned char colour; // its trail's colour on the radar: its index in the radar's trail list
} hud_marker;

// The radar of every trail round eye (the player), turned with the camera (1st person or iso),
// and a marker on the rim in the direction of each of markers
void hud_draw_minimap(world_point eye, const trail *const *trails, int trail_count, const hud_marker *markers,
					  int marker_count);

// The terminal screen's colour (main.c), shared by its text and its cursor
#define HUD_TERMINAL_GREEN JO_COLOR_RGB(40, 255, 90) // matrix green

// The terminal screen's block cursor: a solid green square over text cell (column, row), the
// same cells as jo_printf. SGL's text font has no solid block, so it's a VDP1 sprite
void hud_draw_text_cursor(int column, int row);

// The prologue's burst of light, centred on (cx, cy) in pixels from the screen's centre (+y down):
// rays from inner to outer pixels out (game/loop_break.h times them)
void hud_draw_burst(int cx, int cy, int inner, int outer);

// The designed crash's tear (game/breach.h): a strip the whole width of the screen, rows top
// to top + height - 1 from the screen's top, near black or pink, over everything
void hud_draw_band(int top, int height, bool pink);

// On foot, the head's lock on a target (game/target.h): an amber bracket round (cx, cy), in pixels
// from the screen's centre (+y down)
void hud_draw_lock(int cx, int cy);

#endif
