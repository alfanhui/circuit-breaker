#ifndef __OBJECT_FOG_VDP1_H__
#define __OBJECT_FOG_VDP1_H__

#include <jo/jo.h>
#include "game/depth_fog.h"

/*
 * Fog for walls and trails, with VDP1's Gouraud shading (the floor has its own: floor_fade_vdp2.c).
 *
 * Gouraud shading gives each corner of a quad a colour, and VDP1 blends between them across
 * the quad, adding the result to the quad's own colour or texture. The four colours sit in a
 * small table in VDP1's RAM (8 bytes), and each quad's attribute (jo_3d_polygon_attribute.gouraud_table) says which table.
 * Corners are fogged by distance (game/depth_fog.h). A wall or trail piece's corners are two
 * levels in one of two orders: those share 512 tables written once at start-up. Any other quad
 * gets its own, rewritten every frame. Quads with no fog at any corner share one that changes nothing.
 *
 * Usage, when object_fog_enabled:
 *   - every frame, before drawing: object_fog_begin_frame
 *   - for each corner: object_fog_level; for each quad: object_fog_set_quad(&its jo_3d_polygon_attribute, ...)
 */

extern bool object_fog_enabled; // set by object_fog_init: false and nothing here does anything

// Call after the textures are loaded (checks they don't reach the tables) and before the walls
// and trails are set up (they check object_fog_enabled).
void object_fog_init(void);

// Where the fade begins and ends along the floor: the floor fog's figures. When the view changes.
void object_fog_set_distances(int fade_start, int fade_end);

// Every frame, before drawing. on = false (the free debug camera): nothing is fogged.
void object_fog_begin_frame(bool on, world_point eye, int forward_x, int forward_y);

// The fog level of a corner on (or above) the floor at x, y
int object_fog_level(int x, int y);

// A Gouraud table for one quad, its corners' levels in the quad's vertex order (vertices[0..3]).
// Returns the value for that quad's jo_3d_polygon_attribute.gouraud_table.
unsigned short object_fog_quad(int level0, int level1, int level2, int level3);

// The same, put in the quad's attribute: its table, and Gouraud shading on only if a corner is
// fogged. (Off, VDP1 draws the quad far quicker: it matters for the big ones close to the camera)
void object_fog_set_quad(jo_3d_polygon_attribute *attribute, int level0, int level1, int level2, int level3);

// A flat colour as fogged at a level, for a small untextured polygon drawn without Gouraud
// (depth_fog_darken): the colour itself, darkened as a Gouraud table at that level would
jo_color object_fog_flat_colour(jo_color colour, int level);

#endif
