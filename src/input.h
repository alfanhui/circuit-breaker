#ifndef __INPUT_H__
#define __INPUT_H__

#include <jo/jo.h>
#include "utils/low_level_input.h"

extern jo_pos3D pos;
extern jo_rot3Df rot;
extern bool first_person;
extern jo_pos3D true_position;
extern jo_pos3D position_1st_person;
extern jo_rot3Df rotation_1st_person;
extern jo_pos3D position_3rd_person;
extern jo_rot3Df rotation_3rd_person;

extern int compass_index;
extern int previous_compass_index;

extern jo_pos2D player1_previous_coordinate;
extern bool create_trail;

void debug_pad1(void);
void debug_buttons(void);
void debug_controller(void);
void gamepad_input(void);

#endif
