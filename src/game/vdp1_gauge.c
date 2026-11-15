#include "vdp1_gauge.h"

void vdp1_gauge_init(vdp1_gauge *g)
{
	g->frames = 0;
	g->unfinished = 0;
	g->outlasted = 0;
	g->commands_max = 0;
	g->over = 0;
	g->busy_percent = 0;
	g->commands = 0;
	g->last_unfinished = 0;
	g->unfinished_total = 0;
}

void vdp1_gauge_frame_start(vdp1_gauge *g, unsigned short edsr, unsigned short lopr)
{
	int commands;

	g->last_unfinished = !(edsr & VDP1_EDSR_BEF);
	if (g->last_unfinished)
	{
		++g->unfinished; // shown unfinished: LOPR is only how far it got, not the list's length
		++g->unfinished_total;
		return;
	}
	// Finished: the last command read was the end command. LOPR is its address / 8, and commands
	// are 32 bytes apart, so >> 2 is its place in the list
	commands = lopr >> 2;
	if (commands > g->commands_max)
		g->commands_max = commands;
}

void vdp1_gauge_frame_end(vdp1_gauge *g, unsigned short edsr)
{
	++g->frames;
	if (!(edsr & VDP1_EDSR_CEF))
		++g->outlasted; // the CPU is done and VDP1 isn't: it's the slower one this frame
}

void vdp1_gauge_second(vdp1_gauge *g)
{
	g->over = g->unfinished;
	g->busy_percent = g->frames > 0 ? g->outlasted * 100 / g->frames : 0;
	g->commands = g->commands_max;
	g->frames = 0;
	g->unfinished = 0;
	g->outlasted = 0;
	g->commands_max = 0;
}
