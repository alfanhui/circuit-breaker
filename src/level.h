#ifndef __LEVEL_H__
#define __LEVEL_H__

#include <jo/jo.h>
#include "utils/3d.h"
#include "game/world.h"
#include "game/wall_clip.h"
#include "game/wall_flicker.h"
#include "game/corridor.h"
#include "game/body.h"
#include "game/frustum.h"
#include "game/drone.h" // drone_box

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
 * SG-9: straight after level_view_begin, before anything of the world's drawn. If a box's side near
 * you covers the whole screen (game/screen_wall.h), jo leaves out every polygon further than it
 * (jo_3d_set_depth_limit); otherwise no limit. Put back to none (-1) once the world's drawn, before
 * the mirror and the HUD (main.c)
 */
void level_screen_wall_limit(void);
/*
 * Whether the slave was handed a share of the city's blocks this frame (level.c, grey_groups_draw).
 * Then main.c waits for it before the sky (whose ring cut changes level_view, which the share
 * reads); in other frames it doesn't: in the corridor the slave's glass tiles and boxes went on
 * while the master drew the sky, and waiting there took the bench's corridor views from 0-16 late
 * frames to 49-68 (2026-11-19)
 */
extern bool level_city_shared;
// Its depth this frame, world units (-1: no wall covers the screen): the debug line's sw
extern int level_screen_wall;

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

/*
 * What stands in the sky and on the horizon (level.c has how): the landmarks, one for every place
 * in the story (game/landmarks.h), and the CD block's disc over the city with the drive's works
 * under it and the lift's beam up to them (game/city.h).
 * side, wall, middle: the corridor's wall, how far it is from the arena's middle, and its middle
 * line across (input.h, foot_cd_hub: known before the corridor's open). frame: counts up, for
 * what moves. place: where you are (PLACE_...): that place's own landmark isn't drawn. in_lift:
 * the lift has you, so its beam's left out (you're in it).
 * Only in the views that look out (1st person, on the cycle or on foot); between
 * slave_draw_begin and slave_draw_end, in the world's matrix
 */
// extras: the landmarks and the A-bus too (false: the draw budget's left them out). The skyline and
// the CD block's disc are always drawn
void level_sky_draw(int side, int wall, int middle, unsigned int frame, int place, bool in_lift, bool extras);

// You were moved by (dx, dy) in one frame (a round's layout rising, the loop slid to the middle):
// the sky goes with you, so it doesn't jump, and eases back (game/sky_slide.h)
void level_sky_slide(int dx, int dy);

// Whether you're on foot out past the corridor's doorway, in the city (as of the frame last
// drawn): the floor's the circuit board there (main.c), and the towers stand on it
bool level_in_city(void);

/*
 * A watchdog drone's beam (drone_draw.c, game/drone.h): a square pyramid, the sides that face the
 * eye drawn see-through (VDP1's mesh: a screen door) in `sheet`, every edge a solid line of `edge`.
 * Cut to this frame's view; in the world's matrix, on foot
 */
void level_beam_draw(const drone_beam *beam, jo_color sheet, jo_color edge);

/*
 * The roofs the drone's scan line lights this frame (game/drone.h, drone_scan_lit), coloured as
 * they're drawn: set before the city's drawn (drone_draw_prepare). Each a lit part of one roof
 */
void level_scan_set(const drone_box *lit, int count, jo_color colour);

// The fog level at a point on the floor (object_fog_level), never past the cap the far city's
// drawn at from up high (level.c, LEVEL_FOG_CAP): a watchdog's fogged by it as the roofs under it
// are. 0 while the fog's off
int level_fog_capped(int x, int y);

// Whether a box (its middle, half its size each way) could be seen this frame: in the view, and in
// the city not behind a nearer, taller tower. After the city's drawn (its skyline's built then)
bool level_box_seen(frustum_point centre, frustum_point half);

// How far (x, y) is from this frame's eye along the floor, no less (the longer way plus half the
// shorter: up to 12% over, no square root). For a model's lighter version far off
int level_reach(int x, int y);

#endif
