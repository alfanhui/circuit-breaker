#include "intro_flight.h"

/*
 * The keys, ~2.1 s apart. x is west +, y south +; the message runs east (-x) along the centre
 * line, its letters 1000 tall across it (input.c). The last key is the spectator camera.
 * Every value that swings the picture moves one way only, start to end: the turn (rz) only
 * right, the camera only further west, south and out, the lead only growing. A key that
 * doubled back (turning away to look at the horizon, then back) read as too much panning.
 */
static const intro_shot keys[INTRO_FLIGHT_KEYS] = {
	// Face down to the floor, north up the screen, held still (follow 0) over the S's bottom
	// bar: the route starts 400 west of the S on the message's centre line (attract_text.c), drops
	// 500 south to the bar and rides in from the left, laying it across the picture (~frame 115)
	{.ox = -700, .oy = 500, .height = 350, .rx = 0, .rz = 0, .roll = 0, .fov = INTRO_DEG(60),
	 .lead = 0, .follow = 0, .fade_start = 800, .fade_end = 2200},
	// Lifting its head, starting to drift after the cycle as the S rises from the bar
	{.ox = -500, .oy = 550, .height = 360, .rx = INTRO_DEG(25), .rz = INTRO_DEG(4), .roll = 0,
	 .fov = INTRO_DEG(60), .lead = 0, .follow = 90, .fade_start = 800, .fade_end = 2200},
	// Alongside: low, south of the message, riding with the cycle as it draws, looking north
	// across the letters
	{.ox = -100, .oy = 1300, .height = 400, .rx = INTRO_DEG(66), .rz = INTRO_DEG(10), .roll = 0,
	 .fov = INTRO_DEG(60), .lead = 0, .follow = INTRO_FOLLOW_FULL, .fade_start = 700, .fade_end = 2500},
	// Banking round to the right, climbing away, still looking down at the cycle (3350 out and
	// 1800 up: 62 degrees from straight down meets the floor about where it is)
	{.ox = 1200, .oy = 3000, .height = 1800, .rx = INTRO_DEG(62), .rz = INTRO_DEG(20), .roll = INTRO_DEG(20),
	 .fov = INTRO_DEG(45), .lead = 300, .follow = INTRO_FOLLOW_FULL, .fade_start = 2500, .fade_end = 7000},
	// The spectator's seat: view.c's SPECTATE_* (and floor_fade_vdp2.c's spectator fog), exactly
	{.ox = 4079, .oy = 7064, .height = 3804, .rx = INTRO_DEG(65), .rz = INTRO_DEG(30), .roll = 0,
	 .fov = INTRO_DEG(30), .lead = 700, .follow = INTRO_FOLLOW_FULL, .fade_start = 9700, .fade_end = 20000},
};

// What each key is, for the debug line (main.c)
static const char *const key_names[INTRO_FLIGHT_KEYS] = {"floor", "lift", "alongside", "bank", "seat"};

/*
 * The slope at key b (a and c the keys either side), doubled: Catmull-Rom's (c - a) / 2, x 2.
 * Limited so the curve can't overshoot (Fritsch and Carlson's rule): where the value turns back
 * at b, or holds still on one side of it, the slope is 0, so it eases to a stop there instead of
 * swinging past; and it's at most 3 times the climb on either side (x 2: 6). A steep fall to a
 * near key overshot: from 10000 high down to 150 then 35, the camera dipped into the floor. The
 * slope depends only on the three keys, so both segments meeting at b agree on it: the camera's
 * speed still has no kink there
 */
static int tangent(int a, int b, int c)
{
	int before = b - a;
	int after = c - b;
	int m = c - a;
	int limit;

	if (before == 0 || after == 0 || (before > 0) != (after > 0))
		return 0;
	limit = 6 * (before < 0 ? -before : before);
	if (6 * (after < 0 ? -after : after) < limit)
		limit = 6 * (after < 0 ? -after : after);
	return m > limit ? limit : m < -limit ? -limit : m;
}

// One value between keys p1 and p2 (p0 and p3 either side), t from 0 to 256: the cubic through
// them with the slopes above (intro_flight.h's formula, when they aren't limited), doubled, x256.
// Horner's rule: ((a3 t + a2) t + a1) t + a0, a shift after each multiply
static int spline(int p0, int p1, int p2, int p3, int t)
{
	int m1 = tangent(p0, p1, p2);
	int m2 = tangent(p1, p2, p3);
	int d = 2 * (p2 - p1);
	int a3 = m1 + m2 - 2 * d;
	int a2 = 3 * d - 2 * m1 - m2;
	int v = ((a3 * t) >> 8) + a2;

	v = ((v * t) >> 8) + m1;
	v = ((v * t) >> 8) + 2 * p1;
	return v >> 1;
}

#define SPLINE(field) shot->field = spline(k0->field, k1->field, k2->field, k3->field, t)

void flight_at(const intro_shot *flight, int count, int segment_frames, int t_step, int frame, intro_shot *shot)
{
	int segment = 0;
	int t;
	const intro_shot *k0;
	const intro_shot *k1;
	const intro_shot *k2;
	const intro_shot *k3;

	if (frame <= 0)
	{
		*shot = flight[0];
		return;
	}
	if (frame >= (count - 1) * segment_frames)
	{
		*shot = flight[count - 1];
		return;
	}
	// Between keys segment and segment + 1: a few subtractions, not a division
	while (frame >= segment_frames)
	{
		frame -= segment_frames;
		segment++;
	}
	t = (frame * t_step) >> 8; // 0..255: x256
	k1 = &flight[segment];
	k2 = &flight[segment + 1];
	// Past either end, the key on the other side stands in: the curve starts and ends at rest
	k0 = segment > 0 ? &flight[segment - 1] : k2;
	k3 = segment + 2 < count ? &flight[segment + 2] : k1;

	SPLINE(ox);
	SPLINE(oy);
	SPLINE(height);
	SPLINE(rx);
	SPLINE(rz);
	SPLINE(roll);
	SPLINE(fov);
	SPLINE(lead);
	SPLINE(follow);
	SPLINE(fade_start);
	SPLINE(fade_end);
}

void intro_flight_at(int frame, intro_shot *shot)
{
	flight_at(keys, INTRO_FLIGHT_KEYS, INTRO_SEGMENT_FRAMES, 65536 / INTRO_SEGMENT_FRAMES, frame, shot);
	// follow is a share, so it stays within 0..256 (the limited slopes keep it there anyway)
	if (shot->follow > INTRO_FOLLOW_FULL)
		shot->follow = INTRO_FOLLOW_FULL;
	if (shot->follow < 0)
		shot->follow = 0;
}

const char *intro_flight_key_name(int key)
{
	if (key < 0)
		key = 0;
	if (key >= INTRO_FLIGHT_KEYS)
		key = INTRO_FLIGHT_KEYS - 1;
	return key_names[key];
}
