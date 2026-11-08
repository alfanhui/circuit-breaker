#ifndef __BREACH_H__
#define __BREACH_H__

/*
 * The designed crash (docs/STORY.md): in round 5 you hit the flicker, and the Watchdog's reset
 * fails. Pure C, unit-tested on the PC: it says what each frame looks like and how far you
 * move; input.c and main.c do it.
 *
 * One part after another:
 *   BREACH_TEAR     the reset tries and fails: the screen glitches, worse and worse. The
 *                   terminal screen ("SH-2 process termin...") flashes in for single frames and
 *                   never gets to finish. Everyone else is frozen
 *   BREACH_THROUGH  the section's gone: a hole, and a corridor behind it (corridor.h). You ride
 *                   on through it, laying nothing, out over the grid tailing off into glass
 *   BREACH_FALL     the cycle goes down: the camera drops low, tips forward and rolls
 *   BREACH_SKID     and crashes to a stop, like a car: the speed dies in half a second, the
 *                   camera jolts and wiggles as the motion stops, and settles at a tilt
 *   BREACH_STOPPED  lying still on the glass floor, where Work RAM begins, until a button's
 *                   pressed (breach_get_up). You lie there BREACH_LIE_FRAMES at least: a press
 *                   sooner is kept, and you get up as soon as they've passed (ignored, it felt
 *                   like a press the game had dropped)
 *   BREACH_RISE     getting up: the camera lifts off the floor, levels out and rises to the
 *                   eye's height, easing in and out
 *   BREACH_ON_FOOT  stood up: from here you walk (walker.h); the breach only keeps the floor white
 *
 * A glitched frame is made of hardware effects that cost next to nothing to draw:
 *   - the 3D picture jumps (SGL's projection centre moves, slWindow): the floor (VDP2) doesn't,
 *     so the layers slip apart
 *   - the colour offset (VDP2 adds the same amount to every pixel on screen): a pink, cyan,
 *     white or dark frame
 *   - bands: full-width strips, black or pink, at random rows (VDP1 quads over everything)
 *   - the terminal screen instead of the world, for the frame
 * Which, and how much, comes from an xorshift seeded the same every time: repeatable for the
 * tests and E2E shots.
 */

#define BREACH_NONE 0
#define BREACH_TEAR 1
#define BREACH_THROUGH 2
#define BREACH_FALL 3
#define BREACH_SKID 4
#define BREACH_STOPPED 5
#define BREACH_RISE 6
#define BREACH_ON_FOOT 7

#define BREACH_TEAR_FRAMES 72	 // 1.2 s: the reset fights for the screen and loses
#define BREACH_THROUGH_FRAMES 60 // 1 s through the hole and on
#define BREACH_FALL_FRAMES 32	 // half a second going down
#define BREACH_SKID_SHIFT 5		 // the skid: 32 frames, so the speed's share left is a shift
#define BREACH_SKID_FRAMES (1 << BREACH_SKID_SHIFT)
#define BREACH_RESET_CHARS_MAX 19 // the reset's text never gets past "SH-2 process termin"

#define BREACH_SPEED_MIN 12 // through the hole at least this fast (units a frame), whatever the cruise
#define BREACH_FALL_ROLL 2048 // the camera's roll at the bottom of the fall (ANGLE): about 11 degrees
#define BREACH_FALL_DIP 3072  // and its tip forward: about 17 degrees
#define BREACH_REST_ROLL 1024 // lying still: about 6 degrees over
#define BREACH_REST_DIP 2730  // and 15 degrees down, at the glass: from lower or more level, the
							  // floor's seen nearly edge on and its hexagons don't show
#define BREACH_DROP 10		  // how far the camera drops as the cycle goes down (the eye is 35 up)
#define BREACH_RUMBLE_MAX 400 // the skid's wiggle (ANGLE) as it starts: about 2 degrees, dying away
#define BREACH_LIE_FRAMES 45  // lying still at least this long (0.75 s) before you can get up
#define BREACH_RISE_SHIFT 6	  // getting up: 64 frames (just over a second), so its share is a shift
#define BREACH_RISE_FRAMES (1 << BREACH_RISE_SHIFT)

#define BREACH_SHIFT_X_MAX 32 // the 3D picture's jump, pixels either way
#define BREACH_SHIFT_Y_MAX 8
#define BREACH_BANDS_MAX 3
#define BREACH_BAND_HEIGHT_MAX 17 // pixels

typedef struct
{
	short top;			// pixels from the screen's top
	short height;		// pixels
	unsigned char pink; // 1: pink, 0: black
} breach_band;

// This frame's glitch: all zero for a clean frame
typedef struct
{
	short shift_x, shift_y;	   // the 3D picture's jump, pixels
	short red, green, blue;	   // the colour offset, -255..255 each
	unsigned char reset_shows; // the terminal screen instead of the world, this frame
	unsigned char band_count;
	breach_band bands[BREACH_BANDS_MAX];
} breach_glitch;

typedef struct
{
	unsigned char phase;   // BREACH_NONE ...
	unsigned short frames; // frames into this phase
	unsigned int random;   // xorshift32 state
	breach_glitch glitch;  // this frame's (breach_update)
	unsigned char wants_up; // a button was pressed lying still, before you could get up
} breach;

void breach_init(breach *b);  // nothing happening
void breach_start(breach *b); // the crash into the flicker: the tear begins

// Each frame: moves the parts on and works out the frame's glitch
void breach_update(breach *b);

// How far the cycle moves this frame (units), riding at cruise: at least BREACH_SPEED_MIN through
// the hole and down, dying to nothing in the skid, nothing before or after
int breach_distance(const breach *b, int cruise);

// The skid's first frame: the hit of the cycle going down (a jolt, input.c)
int breach_landed(const breach *b);

// A button's been pressed: lying still, you start getting up, or as soon as you've lain there
// long enough. Does nothing before you've stopped
void breach_get_up(breach *b);

// The camera: its roll, its tip downwards (ANGLE: 65536 a turn), how far it's dropped, and the
// wiggle to keep it rumbling at (camera_shake_hold). A lurch now and then in the tear; going down
// in the fall; settling at a tilt through the skid, wiggling as the motion stops; and back to
// level (all 0) getting up
int breach_roll(const breach *b);
int breach_dip(const breach *b);
int breach_drop(const breach *b);
int breach_rumble(const breach *b);

// How many characters of GAME_TERMINAL_TEXT the reset has typed (the tear)
int breach_typed_chars(const breach *b);

#endif
