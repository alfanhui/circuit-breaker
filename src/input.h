#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>
#include "utils/low_level_input.h"

extern jo_pos3Df pos;
extern jo_rot3Df rot;

void debug_buttons(void);
void debug_controller(void);
void gamepad_input(void);

#endif
