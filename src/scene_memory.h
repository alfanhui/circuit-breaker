#ifndef __SCENE_MEMORY_H__
#define __SCENE_MEMORY_H__

/*
 * The arena and Work RAM take turns with the same memory (2026-11-02). High Work RAM is full: the
 * game's static arrays and jo's malloc heap share it with SGL's own areas past 0x060C0000, and
 * every array we add is there for good (SKILLS.md). So the big buffers of each part of the game
 * aren't static: they're taken from jo's heap (jo_malloc) when that part starts, and given back
 * when it's over, and the next part's are taken in their place:
 *
 *   the arena's   the trails' meshes (main.c, ~60 KB), the prologue's message (input.c, ~8 KB).
 *                 Taken entering any part of the arena (input.c's enter_scene); given back in
 *                 the hidden load at the breach (input.c), when the arena's left behind
 *   Work RAM's    the corridor and its meshes (input.c, level.c, ~22 KB): taken as the breach
 *                 starts (the corridor's seen through the hole while the reset tears), given back
 *                 as the arena starts again (a reset after the corridor, or a new game)
 *
 * Each part's draw and update leave out what isn't there (its pointers are NULL). Taking twice,
 * or giving back what's not held, does nothing.
 */

// main.c: the trails' meshes (the player's, the bots', the prologue's message's)
void arena_trail_meshes_take(void);
void arena_trail_meshes_release(void);

// level.c: the corridor's floor and the grey box's meshes
void corridor_meshes_take(void);
void corridor_meshes_release(void);

#endif
