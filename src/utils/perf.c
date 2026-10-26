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

int perf_fps = 0;
int perf_cpu_average = 0;
int perf_cpu_peak = 0;

static volatile int vblanks = 0;
static int frames = 0;
static int cpu_total = 0;
static int cpu_peak = 0;

void perf_vblank(void)
{
	vblanks++;
}

void perf_frame_start(void)
{
	jo_time_set_frc(0);
}

void perf_frame_end(void)
{
	int cpu = jo_time_get_frc() * 100 / FRC_COUNTS_PER_VBLANK;

	frames++;
	cpu_total += cpu;
	if (cpu > cpu_peak)
		cpu_peak = cpu;

	if (vblanks >= VBLANKS_PER_SECOND)
	{
		perf_fps = frames * VBLANKS_PER_SECOND / vblanks;
		perf_cpu_average = cpu_total / frames;
		perf_cpu_peak = cpu_peak;
		vblanks = 0;
		frames = 0;
		cpu_total = 0;
		cpu_peak = 0;
	}
}
