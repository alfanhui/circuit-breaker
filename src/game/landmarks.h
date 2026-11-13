#ifndef __LANDMARKS_H__
#define __LANDMARKS_H__

/*
 * The landmarks (docs/STORY.md, "The hub: seeing the levels from Work RAM"): every place in the
 * story stands on the horizon, seen long before it's reached, from the arena and from the city.
 * The CD block isn't here: it's the disc over its hub, near enough to walk under (game/city.h).
 *
 * How they're drawn, and why. Each is a cutout: a few flat shapes in one upright plane, thousands
 * of units across, standing 16 000 to 25 000 from the arena's middle, drawn as ordinary polygons
 * (level.c). Not a picture in the sky (VDP2): that would cost nothing, but a sky picture is at
 * infinity, so every landmark would keep its bearing wherever you stood, and couldn't be hidden
 * by the arena's walls or the city's towers. As polygons at a real place they shift against each
 * other as you move (true parallax, for nothing more), the walls and towers hide them, and one
 * can be left out when you're there. The price is VDP1's and jo's per polygon (~0.45% of a frame
 * each), so: flat, 2 to 6 shapes each, only those in view, and never in the views that look down.
 * That far off the angle you see one from hardly changes, so flat is enough.
 *
 * Dull on purpose: near-black violets a shade off the sky's, one muted colour each. They're
 * presence, not signals: nothing in play depends on reading them. Only the last, the video
 * output, glows. Each moves a little, slowly, by what its chip does (landmark_shapes).
 *
 * Positions are in the corridor's frame (game/corridor.h): `out` towards the wall the corridor
 * opens in and on past the city, `across` along that wall, both from the arena's middle. So they
 * keep their places round the city whichever wall that is.
 *
 * Pure C: the table and its moving parts are unit-tested on the PC (tests/test_landmarks.c).
 * No division, no sine: a 16-step wave table and shifts
 */

// Where you are, and what each landmark is: one's never drawn from its own place. Work RAM has
// none: it's the city all round, the skyline's rings (level.c, skyline_add), and the CD block's
// is the disc over the city
#define PLACE_ARENA 0	   // the main CPU's loop
#define PLACE_WORK_RAM 1   // the city, and the corridor to it (no landmark: the skyline)
#define PLACE_CD_BLOCK 2   // the disc (not in the table)
#define PLACE_SCSP 3	   // the sound chip
#define PLACE_BACKUP_RAM 4 // the cartridge
#define PLACE_VDP1 5
#define PLACE_VDP2 6
#define PLACE_VIDEO 7 // the video output: the way out

#define LANDMARK_COUNT 6
#define LANDMARK_SHAPES_MAX 6

// A colour as VDP1 has it (RGB555 and the top bit), from 8-bit parts: JO_COLOR_RGB's own sum
#define LANDMARK_RGB(r, g, b) ((unsigned short)(0x8000 | (((b) >> 3) << 10) | (((g) >> 3) << 5) | ((r) >> 3)))

typedef struct
{
	short out, across;		 // its foot's middle, from the arena's middle
	unsigned char faces_out; // 1: its plane lies along `across` (it's seen along `out`); 0: along `out`
	unsigned char place;	 // PLACE_...: not drawn from there
	short half_width, height; // every shape's within this, along its plane and up from 0
} landmark;

extern const landmark landmarks[LANDMARK_COUNT];

// One flat shape: four corners in the landmark's plane (along it from its middle, and up), in
// order round it
typedef struct
{
	short along[4], up[4];
	unsigned short colour;
} landmark_shape;

/*
 * Landmark `which`'s shapes this frame, into shapes (room for LANDMARK_SHAPES_MAX). Returns how
 * many. frame counts up: what moves, moves by it
 *   the arena: the main CPU as a control tower, a clock light blinking on it
 *   the SCSP: a bank of capacitors, their heights riding a wave (a spectrum, its music)
 *   Backup RAM: a cartridge standing in its slot, its label's light failing (the battery)
 *   VDP1: one huge tilted polygon being filled, line by line from the bottom, again and again
 *   VDP2: four planes one over another, each sliding its own way
 *   the video output: a screen, glowing, a dark bar rolling down it
 */
int landmark_shapes(int which, unsigned int frame, landmark_shape *shapes);

#endif
