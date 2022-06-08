#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>

extern jo_pos3Df pos;
extern jo_rot3Df rot;
extern float boundary;

void gamepad_input(void);

#endif
