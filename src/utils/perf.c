#include <jo/jo.h>
#include "perf.h"

// VDP1's status registers (SEGA's VDP1 manual, chapter 4), read-only, words. game/vdp1_gauge.h
// says what each bit means
#define VDP1_EDSR (*(volatile unsigned short *)0x25D00010) // transfer end status
#define VDP1_LOPR (*(volatile unsigned short *)0x25D00012) // last command read, the frame before
#define VDP1_MODR (*(volatile unsigned short *)0x25D00016) // mode status: the version in bits 15-12
// jo-engine 2026 builds each frame's commands in one of two tables, at 0x20 and 0x8000 in VDP1 RAM,
// turn about. LOPR is an address / 8, so a command in the second table reads 0x1000 more: the low
// 12 bits are its place from its own table's start (one more in the first table, from 0x20)
#define LOPR_IN_TABLE(lopr) ((unsigned short)((lopr) & 0x0FFF))

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
int perf_frame_cpu = 0;
int perf_late_frames = 0;
int perf_frame_late = 0;
int perf_displayed_polygons = 0;
int perf_last_frame_vblanks = 1;
vdp1_gauge perf_vdp1; // zeroed as BSS: every count and figure starts at 0
int perf_vdp1_version = 0;

static int frames = 0;
static int frame_counts = 0; // FRC counts the frames took between them, this second
static int cpu_total = 0;
static int cpu_peak = 0;
static int timing = 0; // 0 until the first frame has started: nothing before it to time
static unsigned short frame_frc = 0; // the FRC at this frame's start

int perf_part_peak[PERF_PART_COUNT];
static int part_counts[PERF_PART_COUNT]; // FRC counts each part took so far this frame
static int part_worst[PERF_PART_COUNT];	 // the most a part took in one frame, this second

void perf_part_add(int part, int frc_before)
{
	part_counts[part] += (unsigned short)(jo_time_get_frc() - frc_before); // (a 16-bit difference: it wraps)
}

int perf_px_peak[PX_COUNT];
#ifdef JO_COMPILE_WITH_PROFILER_SUPPORT
static unsigned int px_master_at = 0, px_slave_at = 0; // each CPU's running sum at the last reading
static unsigned int px_frame[PX_COUNT];				   // this frame's, by part
static unsigned int px_worst[PX_COUNT];				   // the worst frame's, this second

// jo's running sums of the pixels it estimated (never reset: differences are taken). The slave's
// is read through the cache-through address: the slave writes it, and the master's cache could
// hold an old copy (jo's own profiler reads it the same way)
static unsigned int px_master_sum(void)
{
	return __jo_profile_cpu[0].sum[JO_PROFILE_3D_PIXELS];
}
static unsigned int px_slave_sum(void)
{
	return ((volatile jo_profile_cpu *)JO_CACHE_THROUGH(&__jo_profile_cpu[1]))->sum[JO_PROFILE_3D_PIXELS];
}
#endif

void perf_px_mark(int part)
{
#ifdef JO_COMPILE_WITH_PROFILER_SUPPORT
	const unsigned int now = px_master_sum();

	px_frame[part] += now - px_master_at;
	px_master_at = now;
#else
	(void)part;
#endif
}

// At the frame's end: the slave's share and the whole, then each part's worst
static void px_frame_end(void)
{
#ifdef JO_COMPILE_WITH_PROFILER_SUPPORT
	const unsigned int slave = px_slave_sum();

	perf_px_mark(PX_OTHER); // the master's unmarked rest
	px_frame[PX_SLAVE] = slave - px_slave_at;
	px_slave_at = slave;
	px_frame[PX_TOTAL] = 0;
	for (int i = 0; i < PX_SLAVE + 1; ++i)
		px_frame[PX_TOTAL] += px_frame[i];
	for (int i = 0; i < PX_COUNT; ++i)
	{
		if (px_frame[i] > px_worst[i])
			px_worst[i] = px_frame[i];
		px_frame[i] = 0;
	}
#endif
}

// Once a second: the worst frames become the figures shown, in thousands (>> 10: near enough)
static void px_second(void)
{
#ifdef JO_COMPILE_WITH_PROFILER_SUPPORT
	for (int i = 0; i < PX_COUNT; ++i)
	{
		perf_px_peak[i] = (int)(px_worst[i] >> 10);
		px_worst[i] = 0;
	}
#endif
}

/*
 * The frame rate is timed by the FRC (the SH-2's free running counter, a hardware clock): from
 * one frame's start to the next, that's how long the frame really took, drawing and waiting
 * included, with no interrupt involved. (It used to be frames counted against vblanks counted by
 * an interrupt; the two agreed when compared, but this one also says how long each frame took.)
 * The FRC is 16 bits: it wraps after 65535 counts, ~4.7 vblanks, far longer than any frame.
 *
 * It's read, never reset: every time is a 16-bit difference from a reading. jo times with it too
 * (jo_get_ticks, and the profiler's VDP1 drawing time from one interrupt to another): it was set
 * to 0 here each frame until 2026-11-15, which broke their readings across our frame's start
 */
void perf_frame_start(void)
{
	const unsigned short now = (unsigned short)jo_time_get_frc();
	const int period = (unsigned short)(now - frame_frc); // since the last frame's start: the whole of the last frame

	frame_frc = now;
	vdp1_gauge_frame_start(&perf_vdp1, VDP1_EDSR, LOPR_IN_TABLE(VDP1_LOPR)); // did VDP1 finish the frame before?
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
	/*
	 * Late: the fields that showed VDP1's picture before again, by jo's own count. A frame over a
	 * field isn't always one: after a frame of 1.2 fields, a short one's table waits for its own
	 * vblank, so every picture still stays exactly one field. jo's author measured it field by
	 * field (johannes-fetz/joengine#110): jo_core_get_late_frame_count() is the repeated fields.
	 * (We'd counted every frame past a field here, which said 141 late of 192 at 0650 where the
	 * judder seen was VDP2's planes, fixed in jo by #111.) A frame over budget shows in cpu and
	 * cpu peak instead
	 */
	{
		static unsigned int jo_late_before = 0;
		const unsigned int jo_late = jo_core_get_late_frame_count();

		perf_frame_late = (int)(jo_late - jo_late_before);
		jo_late_before = jo_late;
	}
	perf_late_frames += perf_frame_late;
	frame_counts += period;
}

void perf_frame_end(void)
{
	int cpu = (unsigned short)(jo_time_get_frc() - frame_frc) * 100 / FRC_COUNTS_PER_VBLANK;

	// The polygons jo passed on to VDP1 this frame (the debug line shows it a frame late). Read
	// here: jo-engine 2026 zeroes its counts once the frame's commands are sent, before the next
	// frame starts (under SGL they lasted till then)
	perf_displayed_polygons = jo_3d_get_displayed_polygon_count();
	frames++;
	vdp1_gauge_frame_end(&perf_vdp1, VDP1_EDSR); // the CPU's done: is VDP1?
	perf_frame_cpu = cpu;
	cpu_total += cpu;
	if (cpu > cpu_peak)
		cpu_peak = cpu;
	for (int i = 0; i < PERF_PART_COUNT; ++i)
	{
		if (part_counts[i] > part_worst[i])
			part_worst[i] = part_counts[i];
		part_counts[i] = 0;
	}
	px_frame_end();

	// Once a second (of frame time): frames per second, rounded. Divisions once a second only
	if (frame_counts >= FRC_COUNTS_PER_SECOND)
	{
		perf_fps = (frames * FRC_COUNTS_PER_SECOND + (frame_counts >> 1)) / frame_counts;
		perf_cpu_average = cpu_total / frames;
		perf_cpu_peak = cpu_peak;
		vdp1_gauge_second(&perf_vdp1);
		perf_vdp1_version = VDP1_MODR >> 12; // 0 lacks the Pclp and HSS speed bits (the VDP1 manual's supplement)
		frames = 0;
		frame_counts = 0;
		cpu_total = 0;
		cpu_peak = 0;
		for (int i = 0; i < PERF_PART_COUNT; ++i)
		{
			perf_part_peak[i] = part_worst[i] * 100 / FRC_COUNTS_PER_VBLANK;
			part_worst[i] = 0;
		}
		px_second();
	}
}
