#ifndef __LEVEL_H__
#define __LEVEL_H__

#include <jo/jo.h>
#include "utils/3d.h"
#include "input.h"

extern const int wall_width;
extern const int wall_height;
extern const int wall_length;
extern const int wall_half_width;
extern const int wall_half_height;

void load_arena_textures(void);
void init_arena_walls(void);
void draw_arena_walls(void);

#endif
