#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>
#include "utils/low_level_input.h"

extern jo_pos3Df pos;
extern jo_rot3Df rot;
extern float boundary;

char compass(int index);
void gamepad_input(void);

#endif
