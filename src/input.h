#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>
#include "utils/low_level_input.h"
#include "game/cycle.h"
#include "game/arena.h"
#include "game/trail.h"
#include "view.h"

extern cycle player1;
extern trail player1_trail;
extern bool debug_camera;

void init_players(void);
void debug_pad1(void);
void gamepad_input(void);

#endif
