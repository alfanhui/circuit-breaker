#ifndef __HUD_H__
#define __HUD_H__

#include <jo/jo.h>
#include "game/cycle.h"

/*
 * On-screen readouts for the player (the debug text is separate: main.c).
 * So far: the boost gauge, a bar at the bottom centre of the screen.
 */

// Makes the HUD's textures. Once, at start-up, after the other textures are loaded.
void hud_init(void);

// Every frame, after the world is drawn
void hud_draw_boost(const cycle *c);

#endif
