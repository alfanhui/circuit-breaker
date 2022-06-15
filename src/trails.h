#ifndef __TRAILS_H__
#define __TRAILS_H__

#include <jo/jo.h>
#include "utils/3d.h"

extern jo_pos3Df pos;
extern jo_rot3Df rot;

void load_player1_trail_textures(void);
void calculate_player1_trails(void);
void init_player1_trails(void);
void draw_player1_trails(void);

#endif
