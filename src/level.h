#ifndef __LEVEL_H__
#define __LEVEL_H__

#include <jo/jo.h>
#include "utils/3d.h"
#include "game/world.h"
#include "game/wall_clip.h"
#include "game/wall_flicker.h"
#include "game/corridor.h"

void load_arena_textures(void);
// The broken glass over the hole behind you, from the disc: loaded in the hidden load at the
// breach (input.c), not at start-up. Only once
void load_corridor_glass(void);
void init_arena_walls(void);

// Call inside view_push_world_matrix(). Draws the wall pieces inside view (see view_cull.h),
// grouped into longer pieces with distance from eye (arena.h).
// camera: the 1st person camera, to keep wall corners in front of it (wall_clip.h); NULL otherwise.
// cutaway: iso view, where the walls between the camera and the cycle are drawn low.
// rise: how far the walls have risen out of the floor, 0 to 256 (full height): they rise as the
// camera comes down into the arena (game/arrival_flight.h)
// flicker: the round's glitching section of wall (game/wall_flicker.h), or NULL for none
// passage: the corridor behind it (game/corridor.h), its walls drawn once it's open; or NULL
void draw_arena_walls(const world_rect *view, const world_point *eye, const wall_clip_camera *camera, bool cutaway,
					  int rise, const wall_flicker *flicker, const corridor *passage);

// Call inside view_push_world_matrix(), after the floor and before the walls: the corridor's
// floor of dark glass hexagons (game/corridor.h), once it's open
// camera: the 1st person camera (wall_clip.h), to cut only the tiles near it; NULL cuts them all
void draw_corridor_floor(const corridor *passage, const wall_clip_camera *camera);
// Straight after it: the grey box's tops (corridor.h). Their sides go with the corridor's walls
void draw_corridor_boxes(const corridor *passage, const wall_clip_camera *camera);

// Call inside view_push_world_matrix(), with the walls. The raised ground round the arena
// (game/surround.h), level with the walls' tops and rising with them (rise, as above); in the
// iso cutaway, as low as the near walls' base behind them. Only drawn where the camera's above it
void draw_arena_surround(int rise, bool cutaway);

#endif
