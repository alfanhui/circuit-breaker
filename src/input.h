#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>
#include "utils/low_level_input.h"
#include "game/rider.h"
#include "game/game_state.h"
#include "game/status_message.h"
#include "game/loop_break.h"
#include "game/rounds.h"
#include "game/arena.h"
#include "game/trail.h"
#include "view.h"

extern rider player1;
// The computer riders (game/bot.h): as many as the round has (game/rounds.h), the rest out of
// the field (not alive, their trails sunk)
#define BOT_COUNT_MAX 3
extern rider bots[BOT_COUNT_MAX];
extern int bot_skill_level; // how hard they play: BOT_EASY, BOT_MEDIUM or BOT_HARD (game/bot.h)
extern game_state game; // playing, or terminated after a crash (game/game_state.h)
extern status_message status; // a line typed out during play (game/status_message.h)
extern rounds arena_rounds; // which round, and the walls sinking and rising between them (game/rounds.h)
#define FIELD_TRAIL_COUNT (1 + BOT_COUNT_MAX) // you, then the bots: the radar's colours in order
extern const trail *const field_trails[FIELD_TRAIL_COUNT]; // every rider's trail: the radar's colours
// The trails that stop cycles and can hide others: only live riders' (a dead bot's sinks away)
extern const trail *solid_trails[FIELD_TRAIL_COUNT];
extern int solid_trail_count;
extern bool debug_camera;
// Debug: the game moves on 1 frame in DEBUG_SLOW_MOTION_FRAMES (the picture is still redrawn every frame)
#define DEBUG_SLOW_MOTION_FRAMES 8
extern bool debug_slow_motion;
// Debug (debug camera, Z): a crash stops the cycle but doesn't terminate the player
extern bool debug_no_termination;
// The prologue's loop (input.c): the strokes of "SEGA SATURN SH-2" written so far, each with
// its word, and how tall each word's strokes stand (they sink once it's spelt)
extern trail attract_strokes[];
extern unsigned char attract_stroke_word[];
extern int attract_stroke_count;
extern unsigned char attract_word_height[];
// The opening flight's speed: 1, or 3 once a button has hurried it (input.c)
extern int intro_hurry;
// The presses breaking the loop: their burst of light and flash are drawn in main.c
extern loop_break breaker;
extern world_point loop_break_at; // where the cycle was when the loop broke (the last burst is there)

// How far the arena's walls stand, 0 to 256: rising in the arrival, sinking and rising between rounds
int arena_wall_rise(void);

void init_players(void);
void debug_pad1(void);
void gamepad_input(void);

#endif
