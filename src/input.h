#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>
#include "utils/low_level_input.h"
#include "game/rider.h"
#include "game/arena.h"
#include "game/trail.h"
#include "view.h"

extern rider player1;
extern rider bot1; // a computer rider (game/bot.h)
#define FIELD_TRAIL_COUNT 2
extern const trail *const field_trails[FIELD_TRAIL_COUNT]; // every rider's trail
extern bool debug_camera;
// Debug: the game moves on 1 frame in DEBUG_SLOW_MOTION_FRAMES (the picture is still redrawn every frame)
#define DEBUG_SLOW_MOTION_FRAMES 8
extern bool debug_slow_motion;

void init_players(void);
void debug_pad1(void);
void gamepad_input(void);

#endif
