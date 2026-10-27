#ifndef __LEVEL_H__
#define __LEVEL_H__

#include <jo/jo.h>
#include "utils/3d.h"
#include "game/world.h"
#include "game/wall_clip.h"

void load_arena_textures(void);
void init_arena_walls(void);

// Call inside view_push_world_matrix(). Draws the wall pieces inside view (see view_cull.h),
// grouped into longer pieces with distance from eye (arena.h).
// camera: the 1st person camera, to keep wall corners in front of it (wall_clip.h); NULL otherwise.
// cutaway: iso view, where the walls between the camera and the cycle are drawn low.
void draw_arena_walls(const world_rect *view, const world_point *eye, const wall_clip_camera *camera, bool cutaway);

#endif
