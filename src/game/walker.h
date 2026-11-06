#ifndef __WALKER_H__
#define __WALKER_H__

#include "world.h"

/*
 * On foot (docs/STORY.md: Work RAM). Pure C, unit-tested on the PC; input.c reads the pad into
 * walker_controls, view.c turns the camera with it.
 *
 * Off the cycle you're not moving all the time: you stand, and move only while a button's held.
 * The pad (a Saturn pad: a d-pad, two shoulders and six face buttons, no stick):
 *   d-pad left / right   turn (the whole body: where you walk is where you face)
 *   d-pad up / down      walk forwards / backwards
 *   L / R                step sideways (strafe)
 *   A B C / X Y Z        turn the head, as on the cycle: it eases back once let go
 * There's no button left for doing anything: on purpose. It's a stealth part: you get through by
 * moving and looking, not by pressing things.
 *
 * The face buttons in their two rows, the head's way for each:
 *       X  Y  Z         up-left    up     up-right
 *       A  B  C         down-left  down   down-right
 * Two in a column cancel up and down: A + X together look straight left, C + Z straight right.
 *
 * Weight: nothing starts or stops dead. Speeds ramp up over a few frames and down over fewer
 * (stopping quicker than starting reads as control, the other way round as ice). Turning starts
 * slowly, for small adjustments to where you face, and speeds up the longer it's held, for
 * turning right round. Releasing it stops the turn at once: overshooting where you aimed is the
 * worst feeling a turn can have.
 *
 * Positions are kept x256 (WALKER_SUB): a walking speed of 3 units a frame starts at 0.375 in
 * its first frame, which whole units would round away. Angles are SGL's (ANGLE): 0x10000 a full
 * turn, 0 facing north (-y), and turning right goes towards east (-x), as the headings (world.h).
 */

#define WALKER_SUB_SHIFT 8 // positions and speeds x256
#define WALKER_SUB (1 << WALKER_SUB_SHIFT)
/*
 * The scale: the cycle's 50 tall, so a unit's about 2.3 cm. Riding, the eye's 35 up (view.c);
 * stood up, it's WALKER_EYE: 1.55 m. Getting up from the crash leaves you at the cycle's 35, then
 * you straighten to it over half a second (walker_eye)
 */
#define WALKER_EYE 68
#define WALKER_EYE_RIDING 35
#define WALKER_HEAD 76		 // the top of your head above your feet: what you need under a bar to walk
#define WALKER_EYE_SLIDE 26	 // sliding: the eye low, near the floor
#define WALKER_SLIDE_HEAD 34 // and your head: you slide under a bar higher than this
#define WALKER_RADIUS 40 // how near a wall you can stand (world units)
#define WALKER_FOOT 16	 // your footprint, round your middle: you step up as it reaches a step
#define WALKER_WAIST 48	 // a roof up to this above your feet stops your feet (WALKER_FOOT off it);
						 // taller, your body (WALKER_RADIUS off it). Stairs: three steps up are
						 // within the radius, but only one within your foot

// Speeds, world units a frame x256. The cycle cruises at 7 units a frame
#define WALKER_FORWARD_SPEED 768  // 3 a frame: a grid cell (250) in 1.4 s
#define WALKER_BACK_SPEED 640	  // 2.5: backing off is slower, you can't see where you're going,
								  // but not much: it's how you back up for a run up (1.75 was a chore)
#define WALKER_STRAFE_SPEED 576	  // 2.25
#define WALKER_SPEED_UP 96		  // a frame: full walking speed in 8 frames
#define WALKER_SLOW_DOWN 160	  // a frame: stopped from full in 5
#define WALKER_DIAGONAL_X256 181  // walking and strafing together: each x 1 / sqrt 2, so it's no faster

/*
 * Running (docs/WORK_RAM.md, "Moving"). There's no button for it: it's how long you've held
 * forward. Walking straight on at walking speed, you go on speeding up, slowly, to a run: full
 * speed about 0.75 s from standing. Strafing, backing off, turning hard (a turn held up to its
 * full speed: about 15 degrees) or running into a wall or a lip head on drops you back to a walk.
 * A tap of forward never gets past a creep, so the ramp's the stealth control, and the run's loud.
 *
 * Only at a full run, head on, do edges and obstacles carry you on, with no button (Mirror's
 * Edge's flow). Each is worked out whole before it starts (flown or slid on a copy of you), so
 * it's predictable: if it took you once, it always will. Otherwise you stop, as walking:
 *   - a lip: you jump off it, WALKER_VAULT_HOP up, and fly on at your speed, if where you'd come
 *     down is a clean landing (vault_off). Pushing on at a lip you stopped at steps you off
 *   - a bar you'd hit your head on, with room under it to slide (its bottom higher than
 *     WALKER_SLIDE_HEAD): you slide under it, low, for WALKER_SLIDE_FRAMES, slowing to a walk.
 *     Only if you'll come out the far side with room to stand (slide_under)
 *   - something no higher than your waist, thin enough to clear: you vault over it (hands on its
 *     top, a small hop: WALKER_VAULT_OVER_HOP) and land beyond, running on (vault_over)
 *   - something no higher than your waist, too deep to clear: you vault up onto it in a stride
 *   - a ledge up to WALKER_CLIMB: you climb up onto it. Slower: the eye follows at half the pace,
 *     and you're down to a walk at the top (climb). Higher is a wall
 * Your feet go up at once in all of them; the eye follows (the step's lag, below). Later, hands
 * and arms will show which move it is
 */
#define WALKER_RUN_SPEED 1536		// 6 a frame: twice walking
#define WALKER_RUN_SPEED_UP 20		// a frame, past walking speed: a run 38 frames after walking
#define WALKER_VAULT_HOP 1216		// x256 a frame upwards, leaving a lip: 45 up at the top of the
									// jump (a little under your waist: plainly a jump, not a step),
									// 38 frames in the air on the level, a gap of ~225 (5.2 m). It
									// was 18 up and ~140: from the eye it hardly read as a jump
#define WALKER_VAULT_OVER_HOP 384	// the hop off the top of what you vault over: 1 up, clearing it
#define WALKER_CLIMB 100			// the highest you climb up onto at a run: 2.3 m, a ledge your
									// hands reach (your head's at 76: 1.75 m)
#define WALKER_SLIDE_FRAMES 36		// a slide: 0.6 s, ~155 along, slowing from a run to a walk
#define WALKER_SLIDE_FRICTION 24	// x256 a frame off your speed, sliding
#define WALKER_FLIGHT_FRAMES_MAX 64 // how far ahead a jump's worked out: a longer flight isn't taken

/*
 * The eye on steps: your feet go up a step (or down one) at once, so where you stand stays exact,
 * but the eye follows them over a few frames, a quarter of the way each, as Quake's does. Stairs
 * glide, not jolt 8 at a time, and a vault up onto a low wall is a climb. walker_step_lag is how
 * far the eye's still below where it's going (negative: above it, stepping down)
 */
#define WALKER_LAG_MAX 112 // a climb's 100, and a little

// Turning (ANGLE a frame)
#define WALKER_TURN_FIRST 96	  // 0.5 degrees: the first frame of a tap, for fine aiming
#define WALKER_TURN_SPEED_UP 40	  // added each frame it's held
#define WALKER_TURN_MAX 448		  // 2.5 degrees: 90 degrees in about 0.7 s, once it's up to speed
								  // (3.5, half a second, felt a bit quick)

/*
 * Heights (docs/WORK_RAM.md, "Moving"): the floor is roofs at different heights. You go from one
 * to the next only if it's no more than WALKER_STEP_UP above you: a kerb's height (37 cm; a
 * stair's 8, 18 cm). Higher is a
 * wall. Lower is a lip: walking into it stops you there, as a wall does, so you never walk off a
 * roof by accident. Keep pushing for WALKER_DROP_PUSH_FRAMES and you step off, and fall. Not
 * over nothing, though: if there's no roof below to land on, the lip holds however long you
 * push (a street is a fall to nothing, and a fall you didn't mean is the worst feeling there is).
 * Landing from up to WALKER_CLEAN_DROP is clean: the eye dips and you carry on. Further is a long
 * drop: you stagger, held to a creep for WALKER_STAGGER_FRAMES (and, later, the drone hears it).
 * A clean drop's up to 2.8 m: about hanging from a lip and letting go.
 * Falling: WALKER_GRAVITY a frame faster each frame (x256), at most WALKER_FALL_MAX. That's
 * twice real gravity at this scale, as games' falls are: a 100 drop takes 28 frames, a 160 one
 * 35. You keep the speed you stepped off with, and can't steer in the air (you can turn to look,
 * but you go on the way you left)
 */
#define WALKER_STEP_UP 16
#define WALKER_DROP_PUSH_FRAMES 18 // 0.3 s
#define WALKER_CLEAN_DROP 120
#define WALKER_STAGGER_FRAMES 90   // 1.5 s
#define WALKER_CREEP_SPEED 256	   // staggering: 1 a frame
#define WALKER_GRAVITY 64		   // x256: 0.25 a frame, each frame
#define WALKER_FALL_MAX (12 * WALKER_SUB)
#define WALKER_NO_FLOOR (-30000) // walker_area_height: no roof here

// Stepping: the eye bobs as you walk. The step's phase (ANGLE) moves on 1.5 x the speed (x256) a
// frame, and there's a step each half turn of it (0x8000): one every 85 units walked, about two a
// second at full speed. The bob's depth goes with the speed, so it dies away as you stop:
// |sin| x speed >> 18: 3 units walking, 6 at a run
#define WALKER_BOB_SHIFT 18

// The places you can stand: a few axis-aligned rectangles of floor, each at its own height (a
// roof), out to its edges. Where two overlap, you're on the higher (if it's no more than a kerb
// up: any higher, and it's a wall). Two that touch you walk between, if they're a kerb apart,
// and you can stand right at a roof's edge, to look over it. Walls are roofs too: too tall to
// step onto, they keep you WALKER_RADIUS off their sides (a room is a floor, and four tall roofs
// round it). Where there's nothing at all is a lip over nothing: it never lets you go.
// Most roofs go all the way down (a tower, a crate). A bar has a bottom too: it floats, and you
// pass under it if your head clears its bottom (a duct, a pipe: walking, WALKER_HEAD; sliding,
// WALKER_SLIDE_HEAD). Its top is a roof like any other
#define WALKER_AREA_RECTS_MAX 64 // the corridor (5), its grey box (25) and the city's first towers (21)

typedef struct
{
	unsigned char count;
	world_rect rects[WALKER_AREA_RECTS_MAX];
	short heights[WALKER_AREA_RECTS_MAX];
	short bottoms[WALKER_AREA_RECTS_MAX]; // WALKER_NO_FLOOR: all the way down
} walker_area;

// One frame's pad, each -1, 0 or 1
typedef struct
{
	signed char turn;	// -1 left, 1 right
	signed char move;	// 1 forwards, -1 backwards
	signed char strafe; // -1 left, 1 right
	signed char look_x; // the head: -1 left, 1 right
	signed char look_y; // -1 down, 1 up
} walker_controls;

typedef struct
{
	int x, y;		  // x256
	int yaw;		  // ANGLE, 0 .. 0xFFFF
	int turn_speed;	  // ANGLE a frame, 0 while not turning (the sign's the turn's)
	int forward;	  // speed along where you face, x256 a frame (negative: backing off)
	int side;		  // speed to the right, x256 a frame (negative: left)
	int stride;		  // the step's phase (ANGLE): the eye's bob
	int z;			  // how high your feet are, x256
	int fall_speed;	  // falling: downwards, x256 a frame (0 standing)
	int fall_from;	  // where the fall began, or the jump's top (whole units): how far it was, on landing
	int take_off;	  // where you left the roof from (whole units): a landing's drop is from here, so
					  // a jump's hop doesn't make a clean drop a long one
	int air_dx;		  // the way you're going in the air, x256 a frame: as you left the roof
	int air_dy;
	unsigned char falling;
	unsigned char push;	   // frames pushed against a lip so far
	unsigned char stagger; // frames left of a long drop's stagger
	unsigned char dip;	   // the eye's dip from a landing, dying away (whole units)
	unsigned char eye;	   // how high the eye is above your feet: straightening up to WALKER_EYE
	signed char lag;	   // the eye's lag behind a step up (or down): walker_step_lag
	unsigned char slide;   // frames left of a slide under a bar (0: not sliding)
} walker;

void walker_area_clear(walker_area *a);
// Adds the floor between two corners (either order), at height 0. Returns false when there's no
// room for another
int walker_area_add(walker_area *a, int x0, int y0, int x1, int y1);
// The same, as a roof at a height
int walker_area_add_roof(walker_area *a, int x0, int y0, int x1, int y1, int height);
// A bar: from `bottom` up to its top at `height` (a roof you can also pass under)
int walker_area_add_bar(walker_area *a, int x0, int y0, int x1, int y1, int bottom, int height);
// Whether a point's somewhere you can stand (at any height)
int walker_area_holds(const walker_area *a, int x, int y);
// The highest roof at a point (whole units), WALKER_NO_FLOOR if there's none
int walker_area_height(const walker_area *a, int x, int y);

// Stood still at a point, facing yaw, on the floor at height 0
void walker_init(walker *w, world_point at, int yaw);
// Stood on a roof: the same, at a height
void walker_init_at(walker *w, world_point at, int z, int yaw);

// The head's way from the face buttons held (each 0 or 1), as walker_controls' look_x and look_y
void walker_look_from_buttons(int a, int b, int c, int x, int y, int z, signed char *look_x, signed char *look_y);

// One frame: turn, then move, sliding along any edge of the area rather than stopping at it
// (each axis is tried on its own: walking into a wall at an angle slides you along it)
void walker_update(walker *w, walker_controls controls, const walker_area *area);

// Where you are, in whole world units
world_point walker_position(const walker *w);

// How far the eye's down for the step: 0 standing still, up to 3 walking
int walker_bob(const walker *w);

// How high your feet are (whole units)
int walker_height(const walker *w);

// How far the eye's down from a landing: a few units, dying away over a few frames
int walker_dip(const walker *w);

// How high the eye is above your feet (whole units): WALKER_EYE_RIDING just up from the crash,
// straightening to WALKER_EYE
int walker_eye(const walker *w);

// How far the eye's still below your feet's height plus its own, after a step up (whole units,
// easing to 0); negative after a step down
int walker_step_lag(const walker *w);

// Whether you're sliding (under a bar): the eye's low, and the pad doesn't steer
int walker_sliding(const walker *w);

// sin(angle) x1024, for an ANGLE: a table of a quarter turn, the rest by symmetry, in between by a
// straight line. cos is sin a quarter turn on. No division, no floats
int walker_sin(int angle);
int walker_cos(int angle);

#endif
