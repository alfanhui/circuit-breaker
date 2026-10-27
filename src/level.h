#ifndef __LEVEL_H__
#define __LEVEL_H__

#include <jo/jo.h>
#include "utils/3d.h"
#include "game/world.h"

void load_arena_textures(void);
void init_arena_walls(void);

// Call inside view_push_world_matrix()
void draw_arena_walls(void);

#endif
