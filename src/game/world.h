#ifndef __WORLD_H__
#define __WORLD_H__

/*
 * One world coordinate space shared by game logic and rendering.
 * Pure C: no jo/SGL includes, so it compiles on the host for unit tests.
 *
 * Units are 3D polygon units. The origin is the arena centre, +z is up.
 * Headings follow the original controls: turning right goes N -> E -> S -> W.
 */

#define WORLD_WALL_SEGMENTS 16
#define WORLD_WALL_SEGMENT_HALF 62
#define WORLD_ARENA_HALF (WORLD_WALL_SEGMENTS * WORLD_WALL_SEGMENT_HALF) // 992
#define WORLD_PLAYER_LIMIT (WORLD_ARENA_HALF - 96)						  // keeps the 1st person camera clear of the walls

// With the boundary off a cycle can leave the arena, but positions are shorts (max 32767):
// going past that would wrap to -32768 and teleport across the map, so stop well before it.
#define WORLD_COORD_LIMIT 30000

// Vertical limits, as heights above the floor (+z up). Only cameras move vertically:
// the cycle always sits on the floor.
#define WORLD_FLOOR_Z 0
#define WORLD_CEILING_Z 4000

#define HEADING_NORTH 0
#define HEADING_EAST 1
#define HEADING_SOUTH 2
#define HEADING_WEST 3

#define WORLD_HEADING_DX(h) ((h) == HEADING_EAST ? -1 : (h) == HEADING_WEST ? 1 : 0)
#define WORLD_HEADING_DY(h) ((h) == HEADING_NORTH ? -1 : (h) == HEADING_SOUTH ? 1 : 0)

typedef struct
{
	short x;
	short y;
} world_point;

#endif
