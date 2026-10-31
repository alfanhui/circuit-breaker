#include <jo/jo.h>
#include "perf.h"

// The FRC runs at the master clock / 32 (jo_core_init sets JO_TIME_CKS_32_MODE)
#ifdef JO_PAL_VERSION
#define VBLANKS_PER_SECOND 50
#define FRC_COUNTS_PER_VBLANK 16796 // 26.874 MHz / 32 / 50
#else
#define VBLANKS_PER_SECOND 60
#define FRC_COUNTS_PER_VBLANK 13982 // 26.847 MHz / 32 / 60
#endif
#define FRC_COUNTS_PER_SECOND (FRC_COUNTS_PER_VBLANK * VBLANKS_PER_SECOND)

int perf_fps = 0;
int perf_cpu_average = 0;
int perf_cpu_peak = 0;
int perf_late_frames = 0;
int perf_displayed_polygons = 0;
int perf_last_frame_vblanks = 1;

static int frames = 0;
static int frame_counts = 0; // FRC counts the frames took between them, this second
static int cpu_total = 0;
static int cpu_peak = 0;
static int timing = 0; // 0 until the first frame has started: nothing before it to time

/*
 * The frame rate is timed by the FRC (the SH-2's free running counter, a hardware clock): from
 * one frame's start to the next, that's how long the frame really took, drawing and waiting
 * included, with no interrupt involved. (It used to be frames counted against vblanks counted by
 * an interrupt; the two agreed when compared, but this one also says how long each frame took.)
 * The FRC is 16 bits: it wraps after 65535 counts, ~4.7 vblanks, far longer than any frame
 */
void perf_frame_start(void)
{
	int period = jo_time_get_frc(); // since the last frame's start: the whole of the last frame

	jo_time_set_frc(0);
	perf_displayed_polygons = jo_3d_get_displayed_polygon_count();
	if (!timing)
	{
		timing = 1;
		return;
	}
	// How many vblanks it took, to the nearest: past 1.5 it was 2, past 2.5 3, and so on. Counted
	// up by comparisons rather than divided (the game steps this many times, input.c)
	perf_last_frame_vblanks = 1;
	while (perf_last_frame_vblanks < 4 &&
		   period > perf_last_frame_vblanks * FRC_COUNTS_PER_VBLANK + (FRC_COUNTS_PER_VBLANK >> 1))
		perf_last_frame_vblanks++;
	// Late: the last frame took longer than a vblank (plus a little, for the clock's jitter),
	// so the screen showed a frame twice or more. The division is only on a late frame
	if (period > FRC_COUNTS_PER_VBLANK + (FRC_COUNTS_PER_VBLANK >> 3))
		perf_late_frames += (period + (FRC_COUNTS_PER_VBLANK >> 1)) / FRC_COUNTS_PER_VBLANK - 1;
	frame_counts += period;
}

void perf_frame_end(void)
{
	int cpu = jo_time_get_frc() * 100 / FRC_COUNTS_PER_VBLANK;

	frames++;
	cpu_total += cpu;
	if (cpu > cpu_peak)
		cpu_peak = cpu;

	// Once a second (of frame time): frames per second, rounded. Divisions once a second only
	if (frame_counts >= FRC_COUNTS_PER_SECOND)
	{
		perf_fps = (frames * FRC_COUNTS_PER_SECOND + (frame_counts >> 1)) / frame_counts;
		perf_cpu_average = cpu_total / frames;
		perf_cpu_peak = cpu_peak;
		frames = 0;
		frame_counts = 0;
		cpu_total = 0;
		cpu_peak = 0;
	}
}
