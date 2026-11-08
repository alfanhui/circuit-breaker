#ifndef __VDP1_GAUGE_H__
#define __VDP1_GAUGE_H__

/*
 * How hard VDP1 (the polygon chip) is working, from its own status registers. Pure C,
 * unit-tested on the PC; utils/perf.c reads the registers and hands their values in.
 *
 * Until now a frame was judged "VDP1-bound" by guessing: the frame rate fell while the cpu figure
 * stayed low. SEGA's VDP1 manual (chapter 4) shows VDP1 says so itself:
 *   - EDSR (0x25D00010), the transfer end status register:
 *       bit 1, CEF: VDP1 has reached the list's end command in the frame it's drawing now;
 *       bit 0, BEF: it reached it in the frame before. 0 means that frame was shown unfinished
 *       ("transfer-over": the manual's word for it).
 *   - LOPR (0x25D00012): the address / 8 of the last command it read in the frame before. A
 *     command is 32 bytes, so / 4 again is its place in the list. In a frame it finished, that's
 *     the end command: how many commands the frame had.
 *
 * jo's loop is: our callbacks, then slSynch (waits for the vblank, where VDP1 starts drawing the
 * list we just built). So while our callbacks run, VDP1 is drawing the frame before. Read at the
 * frame's start, BEF says whether VDP1 finished that frame. Read at the frame's end, when the CPU
 * is done, CEF says whether VDP1 had finished too. If it hadn't, VDP1 is the slower of the two.
 *
 * Once a second the counts become the figures on the debug line:
 *   over: frames VDP1 didn't finish, this last second. Anything above 0 is dropped drawing;
 *   busy: % of frames VDP1 was still drawing when the CPU had finished its frame. It says which
 *         chip is the slower, not that a frame was lost:
 *           - busy 100 at 60 fps (over 0): VDP1 takes longer than the CPU but still fits. Cutting
 *             CPU work won't buy headroom there; cutting pixels will;
 *           - busy near 100 with the fps down: VDP1 is what limits the frame rate. Cut pixels
 *             (overdraw, big faces, diagonal fill lines), not CPU;
 *           - busy near 0 with the fps down: the CPU is the limit;
 *   cmd:  the most commands in a finished frame (SGL's own few included): the list's length.
 *
 * First readings, in Mednafen (2026-11-04):
 *   - the corridor's start: busy 0, cmd 238, cpu 76%;
 *   - on the city's roof: busy 100 at 60 fps, cpu 48%. When the CPU was done, VDP1 was on
 *     command 55 of 95;
 *   - looking back at the doorway from the city: 37 fps, busy 94, cpu 82%: both chips are short
 *     there, VDP1 the more.
 * MODR (0x25D00016) read 0x1110 there: VDP1 version 1, with SGL's manual frame change (FCM).
 */

#define VDP1_EDSR_BEF 0x0001 // the frame before reached its end command
#define VDP1_EDSR_CEF 0x0002 // this frame has reached its end command

typedef struct
{
	// Counted over the second so far
	int frames;		  // frames ended
	int unfinished;	  // frames VDP1 didn't finish
	int outlasted;	  // frames VDP1 was still drawing when the CPU finished
	int commands_max; // the longest finished list
	// Last second's figures, for the debug line
	int over;
	int busy_percent;
	int commands;
} vdp1_gauge;

void vdp1_gauge_init(vdp1_gauge *g);

// At the frame's start (after slSynch): the registers EDSR and LOPR as read now
void vdp1_gauge_frame_start(vdp1_gauge *g, unsigned short edsr, unsigned short lopr);

// At the frame's end (the CPU's work done): EDSR as read now
void vdp1_gauge_frame_end(vdp1_gauge *g, unsigned short edsr);

// Once a second: turns the counts into the figures and starts counting again. Its one division
// is once a second, not per frame
void vdp1_gauge_second(vdp1_gauge *g);

#endif
