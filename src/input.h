#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>
#include "utils/low_level_input.h"
#include "game/rider.h"
#include "game/game_state.h"
#include "game/status_message.h"
#include "game/arena.h"
#include "game/trail.h"
#include "view.h"

extern rider player1;
extern rider bot1; // a computer rider (game/bot.h)
extern game_state game; // playing, or terminated after a crash (game/game_state.h)
extern status_message status; // a line typed out during play (game/status_message.h)
#define FIELD_TRAIL_COUNT 2
extern const trail *const field_trails[FIELD_TRAIL_COUNT]; // every rider's trail: the radar's colours
// The trails that stop cycles and can hide others: only live riders' (a dead bot's sinks away)
extern const trail *solid_trails[FIELD_TRAIL_COUNT];
extern int solid_trail_count;
extern bool debug_camera;
// Debug: the game moves on 1 frame in DEBUG_SLOW_MOTION_FRAMES (the picture is still redrawn every frame)
#define DEBUG_SLOW_MOTION_FRAMES 8
extern bool debug_slow_motion;

void init_players(void);
void debug_pad1(void);
void gamepad_input(void);

#endif
