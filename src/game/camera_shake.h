#ifndef __CAMERA_SHAKE_H__
#define __CAMERA_SHAKE_H__

/*
 * A short camera shake: a jolt that dies away. Pure C, unit-tested on the PC.
 *
 * When a bot dies, the camera shakes: something big just happened out there, and the jolt
 * says so without any text (juice: feedback you feel). It's an angle added to the camera's
 * turn (yaw) and tilt (pitch) for a few frames, not a move of the camera itself: at the 1st
 * person camera's 35 units off the floor, a few units' move is hard to see, but a degree of
 * turn swings the whole screen.
 *
 * Each frame the yaw flips side (+, -, +, -: a buzz, like an impact through the handlebars)
 * and the pitch flips every other frame at half the size (a slower bob under it), so the two
 * together don't just slide the picture back and forth along one line. The size shrinks to
 * CAMERA_SHAKE_DECAY sixteenths each frame (a multiply and a shift: no division), and once
 * it's under CAMERA_SHAKE_MIN it stops.
 *
 * With the bot kill's strength (3 degrees) it goes, in degrees:
 *     frame:   1     2     3     4     5     6    ...   14    15
 *     yaw:   +3.0  -2.4  +2.0  -1.6  +1.3  -1.1   ...  -0.2  (stopped)
 * about 0.25 s: a jolt, not a wobble, so it never gets in the way of steering. (1.5 degrees was
 * too subtle to notice.)
 *
 * Angles are SGL's: 0x10000 is a full circle, so 1 degree is ~182.
 */

#define CAMERA_SHAKE_DECAY 13 // sixteenths of the size kept each frame
#define CAMERA_SHAKE_MIN 32	  // 0.18 degrees: below this it stops
#define CAMERA_SHAKE_BOT_KILL 546 // 3 degrees: a bot died

typedef struct
{
	int strength;		 // the size of this frame's jolt (ANGLE); 0 when still
	unsigned char frame; // frames since it started: which way this frame flips
} camera_shake;

// Still: no shake
void camera_shake_init(camera_shake *s);

// Start a shake of this strength. One already going keeps the bigger of the two, so two
// crashes close together don't make it smaller
void camera_shake_start(camera_shake *s, int strength);

// One frame: this frame's yaw and pitch to add to the camera (ANGLE), then it dies away a little.
// Both are 0 once it's still
void camera_shake_step(camera_shake *s, int *yaw, int *pitch);

// 1 while it's shaking (the culling then looks all round: the camera faces off its heading)
int camera_shake_active(const camera_shake *s);

#endif
