#ifndef __PERF_H__
#define __PERF_H__

/*
 * Frame rate and master CPU load, integer only.
 * FPS is frames drawn per second of vblanks (jo_get_ticks drifts: float maths and a 16-bit counter).
 * CPU load is free running counter time between perf_frame_start and perf_frame_end,
 * as a percentage of one display frame, so it shows headroom even while locked at full rate.
 */

extern int perf_fps;
extern int perf_cpu_average; // % of a display frame, averaged over the last second
extern int perf_cpu_peak;	 // worst frame in the last second

void perf_vblank(void);
void perf_frame_start(void);
void perf_frame_end(void);

#endif
