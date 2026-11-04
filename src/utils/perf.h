#ifndef __PERF_H__
#define __PERF_H__

#include "../game/vdp1_gauge.h"

/*
 * Frame rate and master CPU load, integer only.
 * FPS is timed by the SH-2's free running counter, from one frame's start to the next (perf.c).
 * CPU load is free running counter time between perf_frame_start and perf_frame_end,
 * as a percentage of one display frame, so it shows headroom even while locked at full rate.
 */

extern int perf_fps;
extern int perf_cpu_average; // % of a display frame, averaged over the last second
extern int perf_cpu_peak;	 // worst frame in the last second
// Vblanks missed since boot (a frame that took 2 vblanks adds 1), from the frame timing. Never resets, so a single
// hitch shows as a step, where the per-second fps average barely moves.
extern int perf_late_frames;
// How many vblanks the last frame took, to the nearest (1 at a steady 60; up to 4): the game
// steps that many times next frame to keep its speed (input.c)
extern int perf_last_frame_vblanks;
// Polygons SGL actually passed to VDP1 last frame, after throwing away those fully off screen.
// SGL only knows this once it has sorted the frame, so it's sampled at the next frame's start.
extern int perf_displayed_polygons;
// VDP1's own view (game/vdp1_gauge.h): frames it didn't finish, how often it was still drawing
// when the CPU was done, and its command list's length. Updated once a second, like fps
extern vdp1_gauge perf_vdp1;
extern int perf_vdp1_version; // VDP1's chip version (MODR bits 15-12): Mednafen reports 1

/*
 * Parts of the frame timed on their own, to find what the cpu figure is spent on: read the FRC
 * (jo_time_get_frc) before the part, then perf_part_add with that reading after it. A part can
 * be added to several times a frame (the game steps up to 3 times). The debug line shows each
 * part's worst frame in the last second, as a % of a display frame, like the cpu peak.
 */
#define PERF_BOTS 0	  // the bots thinking and riding (input.c)
#define PERF_TRAILS 1 // building the trails' meshes to draw (main.c)
// The level's parts (docs/CULLING.md, step 1): each its own, so a film says where the time went.
// What's left of cpu once they're taken off is the rest (the HUD, SGL's frame, the sky...)
#define PERF_ARENA 2	// the arena's walls and blocks, worked out into strips (level.c)
#define PERF_CORRIDOR 3 // the corridor's walls and the glass behind you (level.c)
#define PERF_SIDES 4	// the grey box's and the city's boxes, sides and tops, cut to the view (level.c)
#define PERF_TOPS 5		// all of draw_corridor_boxes: the boxes, on foot the walls, SGL taking them (main.c)
#define PERF_FLOOR 6	// the corridor's glass floor (main.c, level.c)
#define PERF_BANDS 7	// SGL taking the walls' strips: transforms and sorting (level.c)
#define PERF_WALKER 8	// on foot: the walker's step, its roofs checked (input.c)
#define PERF_GRID 9		// the floor's grid on VDP2: its rotation worked out (main.c)
#define PERF_PART_COUNT 10
extern int perf_part_peak[PERF_PART_COUNT]; // % of a display frame, the worst in the last second

void perf_part_add(int part, int frc_before);

void perf_frame_start(void);
void perf_frame_end(void);

#endif
