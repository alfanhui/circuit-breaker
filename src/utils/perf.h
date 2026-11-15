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
extern int perf_frame_cpu;	 // the last frame's, alone (the benchmark's peak: game/bench.h)
// Fields that showed VDP1's picture before again, since boot: jo's late count (jo_core_get_late_frame_count,
// johannes-fetz/joengine#110). Never resets, so a single hitch shows as a step, where the per-second fps average
// barely moves.
extern int perf_late_frames;
extern int perf_frame_late; // the last frame's alone: fields it repeated a picture (the benchmark's count)
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
#define PERF_MIRROR 10	// on foot: the corridor drawn again in the mirror (main.c, level.h)
#define PERF_DRONE 11	   // on foot: the watchdog's drone flying and looking (input.c)
#define PERF_DRONE_MODEL 12 // its model drawn (drone_draw.c)
#define PERF_DRONE_SCAN 13  // its scan's sheet: the rays, and the sheet and lines cut and drawn (drone_draw.c)
#define PERF_PART_COUNT 14
extern int perf_part_peak[PERF_PART_COUNT]; // % of a display frame, the worst in the last second

void perf_part_add(int part, int frc_before);

/*
 * Pixels VDP1 is given to draw, by part (SG-7/SG-8: VDP1's time is pixels, not polygons). jo
 * estimates each polygon's pixels as it builds its command (JO_PROFILE_3D_PIXELS: lines x pixels
 * a line, hidden or not), and keeps a running sum per CPU. Only in a profiler build
 * (JO_COMPILE_WITH_PROFILER_MODULE = 1 in the makefile, which turns jo's count on): elsewhere
 * perf_px_mark costs nothing and the figures stay 0.
 *
 * perf_px_mark(part) after a group of draws gives that part what the master CPU added since the
 * last mark. What the slave CPU draws (the corridor's and city's floor and boxes, slave_draw.h)
 * is counted on its own, as PX_SLAVE, at the frame's end (the slave's done by then). The debug
 * line shows each part's worst frame in the last second, in thousands of pixels
 */
#define PX_OTHER 0	// before the first mark: the arena's floor plane setup, anything not marked
#define PX_WORLD 1	// the corridor's and city's floor and boxes the master drew itself
#define PX_ARENA 2	// the arena's walls
#define PX_TRAILS 3 // the trails
#define PX_SKY 4	// the landmarks and the CD block's disc (level_sky_draw)
#define PX_DRONE 5	// the watchdog's drone and its scan
#define PX_BODY 6	// your body, from your own eyes
#define PX_MIRROR 7 // the mirror's second pass (the master's share)
#define PX_SLAVE 8	// everything the slave CPU drew
#define PX_TOTAL 9	// the frame's whole (both CPUs)
#define PX_COUNT 10
extern int perf_px_peak[PX_COUNT]; // thousands of pixels, the worst frame in the last second
void perf_px_mark(int part);

void perf_frame_start(void);
void perf_frame_end(void);

#endif
