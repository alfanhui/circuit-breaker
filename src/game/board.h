#ifndef __BOARD_H__
#define __BOARD_H__

/*
 * The circuit board under Work RAM's city: the ground the towers (the memory's cells) stand on,
 * seen on foot between them and below the skyline. Pure C, unit-tested on the PC.
 *
 * It's the VDP2 floor (RBG0), the same plane and the same tile as the arena's grid, so it costs
 * VDP1 and the CPUs nothing to draw. tools/make_floor.py puts both pictures in FLOOR.TGA: each
 * texel's palette index is
 *     board class x BOARD_GRID_SHADES + grid shade
 * The palette as loaded colours by the grid shade alone (the arena's grid). board_palette
 * colours by the board class alone: write it into the floor's palette and the board shows.
 *
 * The traces' texels are BOARD_PHASES classes, by how far along the trace they are. Lighting a
 * few phases and moving them on every BOARD_STEP_FRAMES frames sends pulses of light along every
 * trace at once (palette cycling): no pixel's redrawn, only colours change.
 */

#define BOARD_GRID_SHADES 5 // the arena grid's shades: background, 3 blends, the line

// The board's classes (make_floor.py has the same)
#define BOARD_SUBSTRATE 0  // the board itself, between everything
#define BOARD_TRACE_EDGE 1 // the darker rim either side of a trace
#define BOARD_PAD 2		   // copper: a via's ring, a pad, a stub
#define BOARD_HOLE 3	   // a via's hole
#define BOARD_FIRST_PHASE 4
#define BOARD_PHASES 32 // along a trace: 8 texels each, 125 world units drawn on foot
#define BOARD_CLASSES (BOARD_FIRST_PHASE + BOARD_PHASES)
#define BOARD_INDICES (BOARD_CLASSES * BOARD_GRID_SHADES) // 180 of the palette's 256

/*
 * The pulses: one every BOARD_PULSE_PERIOD phases along a trace (two to a tile: one every 2000
 * world units), BOARD_PULSE_LENGTH phases long, its head brightest. They move a phase every
 * BOARD_STEP_FRAMES frames: 125 units in 4 frames, about 1900 a second, data on the move
 */
#define BOARD_PULSE_PERIOD 16
#define BOARD_PULSE_LENGTH 3
#define BOARD_STEP_SHIFT 2
#define BOARD_STEP_FRAMES (1 << BOARD_STEP_SHIFT)

// A Saturn colour from 5-bit parts (0..31), as VDP2 keeps it: bit 15 set, then blue, green, red
#define BOARD_RGB(r, g, b) ((unsigned short)(0x8000 | ((b) << 10) | ((g) << 5) | (r)))

// The colour of a board class at pulse step `step` (which phase the pulses' heads are on)
unsigned short board_colour(int board_class, unsigned int step);

// The floor's palette for the board at pulse step `step`: its first BOARD_INDICES entries.
// Every grid shade of a class gets the class's colour (the grid's gone)
void board_palette(unsigned short *palette, unsigned int step);

#endif
