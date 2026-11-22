#ifndef __SEEN_GLOW_H__
#define __SEEN_GLOW_H__

#include <stdbool.h>

/*
 * The screen's red glow while a watchdog's beam has you (the user, 2026-11-17: "make the screen
 * glow red when detected (but keep it light subtle)"). Pure C, unit-tested (tests/test_seen_glow.c);
 * main.c hands it to VDP2's colour offset, which adds the same amount to every pixel on screen:
 * nothing more to draw, and no polygons.
 *
 * It eases in fast (SEEN_GLOW_RISE a frame: full in 8 frames, you know at once) and out slowly
 * (SEEN_GLOW_FALL: 32 frames, half a second, so a beam brushing past is still felt). At its full
 * it adds SEEN_GLOW_RED to red and takes SEEN_GLOW_COOL off green and blue: a tint, not a wash
 * (the colour offset's range is -256 to 255 a channel, 0 to 255 being the colours themselves)
 */

#define SEEN_GLOW_FULL 32
#define SEEN_GLOW_RISE 4
#define SEEN_GLOW_FALL 1
#define SEEN_GLOW_RED 28 // 40 and 12 at first: in the film (watchdogs.steps) the grey roofs went pink
#define SEEN_GLOW_COOL 8

typedef struct
{
	unsigned char level; // 0 .. SEEN_GLOW_FULL
} seen_glow;

void seen_glow_init(seen_glow *g);

// Once a frame: up while seen, down while not
void seen_glow_update(seen_glow *g, bool seen);

// The colour offset to add now, each channel. All 0 once it's faded
void seen_glow_offset(const seen_glow *g, int *red, int *green, int *blue);

#endif
