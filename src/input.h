#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>
#include "utils/low_level_input.h"
#include "game/body.h"
#include "game/rider.h"
#include "game/game_state.h"
#include "game/status_message.h"
#include "game/loop_break.h"
#include "game/rounds.h"
#include "game/wall_flicker.h"
#include "game/breach.h"
#include "game/corridor.h"
#include "game/arena.h"
#include "game/trail.h"
#include "view.h"

extern rider player1;
// The computer riders (game/bot.h): as many as the round has (game/rounds.h), the rest out of
// the field (not alive, their trails sunk)
#define BOT_COUNT_MAX 3
extern rider bots[BOT_COUNT_MAX];
extern int bot_skill_level; // the difficulty picked (A in play): BOT_EASY, BOT_MEDIUM or BOT_HARD (game/bot.h)
int bot_skill_now(void);	 // how hard they play this round: the pick, or the round's least if higher
extern game_state game; // playing, or terminated after a crash (game/game_state.h)
extern status_message status; // a line typed out during play (game/status_message.h)
extern rounds arena_rounds; // which round, and the walls sinking and rising between them (game/rounds.h)
extern wall_flicker arena_flicker; // the round's glitching section of outer wall (game/wall_flicker.h)
extern breach arena_breach; // the designed crash, round 5, through the flicker (game/breach.h)
extern corridor *arena_corridor; // behind the flicker, once it's given way (game/corridor.h); NULL
								 // till the breach, in Work RAM's memory (scene_memory.h)
#define FIELD_TRAIL_COUNT (1 + BOT_COUNT_MAX) // you, then the bots: the radar's colours in order
extern const trail *const field_trails[FIELD_TRAIL_COUNT]; // every rider's trail: the radar's colours
// The trails that stop cycles and can hide others: only live riders' (a dead bot's sinks away)
extern const trail *solid_trails[FIELD_TRAIL_COUNT];
extern int solid_trail_count;
// The watchdogs' drones over the city (game/drone.h), from when you're up on foot: drone_draw.c
// draws them
#include "game/city.h" // (and game/drone.h)
#include "game/seen_glow.h"
#if CITY_WATCHDOG
extern drone watchdogs[CITY_WATCHDOGS];
#endif
extern drone_box watchdog_boxes[CITY_TOWERS]; // the towers, for their sight and for their light to stop at
extern seen_glow watchdog_glow;				  // the screen's red while a beam has you (main.c)
// The address of the city tower you stand over, as the floor plan labels it (row << 8 | column << 4:
// docs/images/work_ram_plan.svg), for the debug text; -1 off the city (or not on foot)
int foot_tower_address(void);
// The pause (START): the game frozen, the pause menu up (game/settings.h), and the camera free to
// fly about (the debug camera, which it began as: the name's kept)
extern bool debug_camera;
// The pause menu's on screen: as it pauses, and after L or R; flying the camera puts it away
extern bool pause_menu_up;
// Debug: the game moves on 1 frame in DEBUG_SLOW_MOTION_FRAMES (the picture is still redrawn every frame)
#define DEBUG_SLOW_MOTION_FRAMES 8
extern bool debug_slow_motion;
// (A crash that only stops the cycle, not terminating you, is the pause menu's "No termination",
// or Z while paused: game/settings.h)
// The prologue's loop (input.c): the strokes of "SEGA SATURN SH-2" written so far, each with
// its word, and how tall each word's strokes stand (they sink once it's spelt)
extern trail *attract_strokes; // the arena's memory (scene_memory.h): NULL once it's given back
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
// On foot with the head locked onto a target (game/target.h): where it is on the screen, in pixels
// from the centre (+y down). False if nothing's locked, or it's off the screen
bool foot_target_on_screen(int *screen_x, int *screen_y);
// On foot: how high your feet are (the roof you're on) and the top of your head (lower crouched in a
// slide): for you in the mirror (level.h)
int foot_feet_height(void);
// Where the corridor is, for what stands in the sky (level.h, level_sky_draw): its wall's side,
// how far that is from the arena's middle, and its middle line across. From the corridor once it's
// open; before that, from where round 5's flicker will open it. *in_lift: the CD block's lift has
// you. false: nowhere yet
bool foot_cd_hub(int *side, int *wall, int *middle, bool *in_lift);
bool foot_body_state(body_state *s, int *yaw, int *feet); // your body: input.c has what

// The benchmark's run (BENCH in features.h, game/bench.h): which view it's at and what each cost,
// for main.c to show
#include "game/bench.h"
const bench_run *input_bench(void);

#endif
