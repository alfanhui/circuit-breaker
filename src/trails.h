#ifndef __TRAILS_H__
#define __TRAILS_H__

#include <jo/jo.h>
#include "utils/3d.h"

extern jo_pos3Df pos;
extern jo_rot3Df rot;
extern int compass_index;
extern int previous_compass_index;

extern jo_pos2D player1_previous_coordinate;
extern bool create_trail;

void load_player1_trail_textures(void);
void calculate_player1_trails(void);
void init_player1_x_trails();
void init_player1_y_trails();
void draw_player1_trails(void);

#endif
