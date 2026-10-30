#ifndef __HUD_H__
#define __HUD_H__

#include <jo/jo.h>
#include "game/cycle.h"

/*
 * On-screen readouts for the player (the debug text is separate: main.c).
 * One instrument at the bottom centre of the screen: the 1st person radar, with the boost
 * gauge as its rim (the rim shows in both views).
 */

// Makes the HUD's textures. Once, at start-up, after the other textures are loaded.
void hud_init(void);

// Every frame, after the world is drawn: the gauge, round the radar's rim
void hud_draw_boost(const cycle *c);

// 1st person only: the radar of every trail round eye (the player), turned with the camera
void hud_draw_minimap(world_point eye, const trail *const *trails, int trail_count);

// The terminal screen's colour (main.c), shared by its text and its cursor
#define HUD_TERMINAL_GREEN JO_COLOR_RGB(40, 255, 90) // matrix green

// The terminal screen's block cursor: a solid green square over text cell (column, row), the
// same cells as jo_printf. SGL's text font has no solid block, so it's a VDP1 sprite
void hud_draw_text_cursor(int column, int row);

#endif
