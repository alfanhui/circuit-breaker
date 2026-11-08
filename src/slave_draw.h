#ifndef __SLAVE_DRAW_H__
#define __SLAVE_DRAW_H__

#include <jo/jo.h>

/*
 * Meshes drawn by the Saturn's second CPU, the slave SH-2, while the master carries on.
 *
 * Under SGL a good part of every polygon's work ran on the slave without our asking. jo-engine
 * 2026 does all of it on the master unless told, and on foot that was ~40% of a frame
 * (docs/BACKLOG.md, "Win back the CPU on foot"). So the meshes that cost the most, the corridor's
 * glass floor and its boxes, are handed over as soon as they're built: the slave transforms,
 * cuts and turns them into VDP1 commands while the master builds the next one, then the walls
 * and the trails. jo sorts both CPUs' commands together, so the picture is the same.
 *
 * A frame (main.c's draw_3d):
 *     slave_draw_begin();          // with the world's matrix current: the slave starts from it
 *     slave_draw_mesh(&mesh);      // each mesh, once it's built: drawn by the slave from now on
 *     ...                          // the master's own drawing meanwhile
 *     slave_draw_end();            // waits for the slave: before the world's matrix is popped
 *
 * A mesh handed over must not change until slave_draw_end(): its points, polygons and
 * attributes are read while the master goes on. So only meshes with arrays of their own that are
 * drawn once a frame: not the arena's wall strips, which share their polygons and attributes and
 * are refilled between draws (the wall fog changes the shared attributes as it goes).
 * Without jo's dual CPU module (makefile), slave_draw_mesh simply draws on the master.
 */

// Start a frame's slave drawing: the slave draws with the master's current matrix
void slave_draw_begin(void);

// Hand a built mesh to the slave, to draw with the matrix slave_draw_begin was given
void slave_draw_mesh(const jo_3d_mesh *mesh);

// Wait for the slave to finish everything handed to it this frame
void slave_draw_end(void);

#endif /* !__SLAVE_DRAW_H__ */
