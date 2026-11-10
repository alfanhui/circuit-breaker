#ifndef __LEVEL_H__
#define __LEVEL_H__

#include <jo/jo.h>
#include "utils/3d.h"
#include "game/world.h"
#include "game/wall_clip.h"
#include "game/wall_flicker.h"
#include "game/corridor.h"
#include "game/body.h"

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

// Once a frame, before the corridor's floor and boxes: this frame's view on foot (view_frustum),
// with the corridor's doorway onto the city added to it as planes (level.c: grey_portal).
// free_camera: the debug camera (no cuts: everything's drawn)
void level_view_begin(const corridor *passage, bool free_camera);

/*
 * The mirror over the hole you broke through (level.c has how). After the frame's corridor is
 * drawn (and the slave's finished with its meshes), in the world's matrix:
 *     if (level_mirror_begin(passage, clip, &mirrored))
 *     {
 *         push the matrix; level_mirror_matrix();
 *         level_mirror_draw(passage, &mirrored);
 *         level_body_draw(false); // you, in the glass
 *         pop the matrix; level_mirror_end(passage, clip);
 *     }
 * level_mirror_begin returns false when the glass isn't in view from inside the corridor.
 * level_mirror_end puts the view back, then draws the glass's streaks of light over it all
 */
bool level_mirror_begin(const corridor *passage, const wall_clip_camera *camera, wall_clip_camera *mirrored);
void level_mirror_matrix(void);
void level_mirror_draw(const corridor *passage, const wall_clip_camera *mirrored);
void level_mirror_end(const corridor *passage, const wall_clip_camera *camera);

/*
 * You (game/body.h), drawn from the same joints both times: from your own eyes and in the mirror.
 * Once a frame, after level_view_begin: level_body_set poses the body (state's eye is filled in
 * here: the camera's height above your feet) and places it in the world, facing yaw (the walker's),
 * feet at that height; or level_body_clear when there's no body (on the cycle). Then
 * level_body_draw(true) in the world's matrix (no head: you're in it), and (false) in the mirror's
 */
void level_body_set(body *b, body_state *state, int yaw, int feet);
void level_body_clear(void);
void level_body_draw(bool own_eyes);

// Call inside view_push_world_matrix(), after the floor and before the walls: the corridor's
// floor of dark glass hexagons (game/corridor.h), once it's open
// camera: the 1st person camera (wall_clip.h), to cut only the tiles near it; NULL cuts them all
void draw_corridor_floor(const corridor *passage, const wall_clip_camera *camera);
// Straight after it: the grey boxes (corridor.h) and the city's towers (city.h), sides and tops,
// each face cut to the view in 3D (game/frustum.h). camera: unused (level_view_begin's view)
void draw_corridor_boxes(const corridor *passage, const wall_clip_camera *camera);

// Call inside view_push_world_matrix(), with the walls. The raised ground round the arena
// (game/surround.h), level with the walls' tops and rising with them (rise, as above); in the
// iso cutaway, as low as the near walls' base behind them. Only drawn where the camera's above it
void draw_arena_surround(int rise, bool cutaway);

#endif
