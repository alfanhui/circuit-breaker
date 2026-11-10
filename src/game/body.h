#ifndef __BODY_H__
#define __BODY_H__

/*
 * You, as a body (docs/WORK_RAM.md, "Your body"). On the cycle you were only energy; after the
 * crash you're a person: arms, legs, a faceless head. One skeleton, posed once a frame, and drawn
 * from wherever it's looked at: from your own eyes (the arms and legs that come into view) and in
 * the mirror (all of you). There's no second "arms in the HUD" animation to drift out of step with
 * the body: what your eyes see is the same joints the mirror shows. Pure C, unit-tested on the PC
 * (tests/test_body.c); level.c draws it.
 *
 * How a pose is made: nothing's keyframed. Each frame the mode (standing, in the air, sliding,
 * thrown from the cycle ...) says where the two hands and the two feet want to be; they ease
 * towards there (so every change of mode blends, for free); and the elbows and knees are worked
 * out from them (two bones of known lengths between a shoulder and a hand can only bend one way
 * once you say which side the elbow's on: body.c, solve). So bones never stretch, whatever the
 * pose, and a new move is four points, not an animation.
 *
 * Where the hands go in a vault or a climb comes from where the walker is, never from a block's
 * size: as the move starts your feet are on the top's near edge already (walker.h, walker_move), so
 * the edge is at the body's own ground, just ahead, however high or deep the block is, and the eye
 * is still below it, catching up: in a climb you hang from your hands until it's up over them. In
 * a vault the hand stays on the spot it landed on while you fly on past it (plant: it goes back by
 * your speed each frame), and lets go once that's out of reach behind you. Change the city's blocks and the hands still land.
 *
 * The poses are designed for your own eyes: honest arms are mostly out of sight (why other games
 * fake them), so the run pumps the hands high enough to rise into the bottom of the view, and
 * lying after the crash you lift a hand and look at it. The mirror shows the same, truthfully.
 *
 * Coordinates: the body's own, x16 (BODY_SUB): x to your right, y the way you face, z up from the
 * ground under your eye. The eye's at (0, 0, eye). A unit's ~2.3 cm (walker.h): you're 76 tall
 */

#define BODY_SUB_SHIFT 4
#define BODY_SUB (1 << BODY_SUB_SHIFT)

typedef struct
{
	short x, y, z;
} body_point;

// The joints. Left ones first, the right one BODY_SIDE after: joint + side * BODY_SIDE
enum
{
	BODY_CROWN, // the top of the head
	BODY_BROW,	// its widest, at the eye's height
	BODY_NECK,	// its foot, between the shoulders
	BODY_PELVIS,
	BODY_SHOULDER, // left ...
	BODY_ELBOW,
	BODY_WRIST,
	BODY_KNUCKLES,
	BODY_FINGERS, // their tips
	BODY_HIP,
	BODY_KNEE,
	BODY_ANKLE,
	BODY_TOES,
	BODY_SHOULDER_R, // ... and right, in the same order
	BODY_ELBOW_R,
	BODY_WRIST_R,
	BODY_KNUCKLES_R,
	BODY_FINGERS_R,
	BODY_HIP_R,
	BODY_KNEE_R,
	BODY_ANKLE_R,
	BODY_TOES_R,
	BODY_JOINTS
};
#define BODY_SIDE (BODY_SHOULDER_R - BODY_SHOULDER)

// The bones' lengths (whole units). The limbs a little long: a slick figure, and hands that reach
// into view
#define BODY_UPPER_ARM 14
#define BODY_FOREARM 13
#define BODY_HAND 8 // the palm 3.5 and the fingers 4.5
#define BODY_THIGH 18
#define BODY_SHIN 18
#define BODY_FOOT 7
#define BODY_TORSO 24	   // the neck to the pelvis
#define BODY_EYE_TO_NECK 7 // the neck's this far under the eye
#define BODY_EYE_TO_CROWN 8
#define BODY_SHOULDERS 10 // each shoulder from the middle
#define BODY_HIPS 5

// What a bone's part of: a skin colours each part its own way
enum
{
	BODY_PART_HEAD,
	BODY_PART_TORSO,
	BODY_PART_ARM,
	BODY_PART_HAND,
	BODY_PART_LEG,
	BODY_PART_FOOT,
	BODY_PARTS
};

/*
 * A bone: drawn as one flat strip from a joint to the next, turned to face whoever looks. Its
 * width is given at each end, so a strip tapers (a forearm to its wrist, the torso to the waist):
 * the shape of a limb for no more polygons. Where a bone starts at the joint the one before it in
 * the table ended at (an arm's three, a leg's), the two strips share that end's edge (level.c
 * mitres it), so a limb bends without a notch. The hand is two: the palm widens to the knuckles,
 * the fingers narrow to their tips. The head is two: wide at the brow, narrow at the jaw and crown
 */
typedef struct
{
	unsigned char from, to;
	unsigned char half_from, half_to; // half the strip's width at each end, x16
	unsigned char part;
} body_bone;
#define BODY_BONES 18
extern const body_bone body_bones[BODY_BONES];
#define BODY_BONES_HEAD 2 // the first two: not drawn from your own eyes, you're inside it

/*
 * A skin: how the body looks, apart from how it moves. Each part's fill and the rim of light round
 * it, as Saturn colours (RGB555, bit 15 set). Another look is another of these (and, later, the
 * bones' widths, or a texture for each part): nothing in the poses or the drawing changes
 */
#define BODY_RGB(r, g, b) ((unsigned short)(0x8000 | (((b) >> 3) << 10) | (((g) >> 3) << 5) | ((r) >> 3)))
typedef struct
{
	unsigned short fill[BODY_PARTS];
	unsigned short rim[BODY_PARTS];
} body_skin;
extern const body_skin body_skin_glass; // dark glass, rimmed in cyan light

// What you're doing: input.c says, from the walker and the crash (game/breach.h)
enum
{
	BODY_MOVING, // standing, walking, running: by speed
	BODY_AIR,	 // jumping or falling: arms out for balance
	BODY_SLIDE,	 // under a bar: legs out ahead, leant back
	BODY_BRACE,	 // thrown from the cycle as it goes down: both hands out in front
	BODY_LIE,	 // lying where you stopped; after BODY_LIE_LOOK frames you lift a hand and look at it
	BODY_RISE,	 // getting up: hands on the floor, pushing
	BODY_VAULT,	 // over something low: the left hand on its top as you pass, the legs swung clear
	BODY_CLIMB	 // up onto a ledge: both hands on its edge, hanging from it, then pushing up
};
#define BODY_LIE_LOOK 24

typedef struct
{
	int mode;	// BODY_MOVING ...
	int eye;	// how high your eye is above the ground, x16
	int stride; // the step's phase (ANGLE: the walker's)
	int speed;	// forwards, x256 a frame (the walker's)
} body_state;

typedef struct
{
	body_point hand[2], foot[2]; // where they are now (left, right): easing to where the mode wants them
	body_point joint[BODY_JOINTS];
	unsigned char mode;
	unsigned char mode_frames; // frames in this mode (stops at 255)
	unsigned char posed;	   // 0 until the first body_update: that one doesn't ease, it's there
	short plant;			   // a vault's hand: how far ahead of you the spot it's on is, x16
	short eye_was;			   // the last frame's eye height
} body;
#define BODY_PLANT_AHEAD 10	 // where the hands land as the move starts: on the top, just ahead of you
#define BODY_PLANT_BEHIND 22 // let go once it's this far behind you: at a run, 5 frames on it
#define BODY_CLIMB_AHEAD 20	 // a climb's hands: this far onto the top, where your eyes catch them as they
							 // come up past it (at 10 they were too far up, then down, to see)
#define BODY_CLIMB_UP 30	 // a climb's hands are on the edge until your eye's this far above the top

void body_init(body *b);

// One frame: the hands and feet ease to where the mode wants them, and every joint's worked out.
// A few square roots and 8 divisions (two a limb, in solve): once a frame, not per polygon
void body_update(body *b, const body_state *s);

#endif
