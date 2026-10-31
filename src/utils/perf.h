#ifndef __PERF_H__
#define __PERF_H__

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

void perf_frame_start(void);
void perf_frame_end(void);

#endif
