#include "arrival_flight.h"

/*
 * The keys after the first (the spectator's camera), from the cycle's start: x is west +, y
 * south +, and the cycle faces north (-y). The arena is 16000 across, centred on the start.
 * As in the opening flight, the turn only goes one way: from the stands' 30 degrees back to 0.
 */
static const intro_shot keys[ARRIVAL_KEYS - 1] = {
	// Overview: high over the south of the arena, looking down at the start (14000 off along the
	// floor, 10000 up: 35 degrees below level), turned 12 degrees east, so the whole arena is in
	// the picture through a 70 degree FOV: the near wall at the bottom, the far one at the top
	{.ox = 2911, .oy = 13694, .height = 10000, .rx = INTRO_DEG(55), .rz = INTRO_DEG(12), .roll = 0,
	 .fov = INTRO_DEG(70), .fade_start = 26000, .fade_end = 40000}, // the far wall's 22000 off: clear
	// Swooping down behind the cycle, still looking down at it and the arena ahead
	{.ox = 0, .oy = 3500, .height = 1600, .rx = INTRO_DEG(65), .rz = INTRO_DEG(3), .roll = 0,
	 .fov = INTRO_DEG(75), .fade_start = 2500, .fade_end = 9000},
	// Just behind and above it, levelling out
	{.ox = 0, .oy = 450, .height = 150, .rx = INTRO_DEG(82), .rz = 0, .roll = 0, .fov = INTRO_DEG(85),
	 .fade_start = 600, .fade_end = 4000},
	// In it: view.c's 1st person camera (FIRST_PERSON_*) and floor_fade_vdp2.c's 1st person fog
	{.ox = 0, .oy = 0, .height = 35, .rx = INTRO_DEG(90), .rz = 0, .roll = 0, .fov = INTRO_DEG(90),
	 .fade_start = 400, .fade_end = 3000},
};

static const char *const key_names[ARRIVAL_KEYS] = {"stands", "overview", "swoop", "behind", "cycle"};

void arrival_flight_at(const intro_shot *from, world_point start, int frame, intro_shot *shot)
{
	// The whole flight, its first key the spectator's camera: 5 keys, 11 values each, on the stack
	intro_shot flight[ARRIVAL_KEYS];

	flight[0] = *from;
	for (int k = 1; k < ARRIVAL_KEYS; ++k)
	{
		flight[k] = keys[k - 1];
		flight[k].ox += start.x; // into the world
		flight[k].oy += start.y;
	}
	flight_at(flight, ARRIVAL_KEYS, ARRIVAL_SEGMENT_FRAMES, 65536 / ARRIVAL_SEGMENT_FRAMES, frame, shot);
}

int arrival_wall_rise(int frame)
{
	// Smoothly: 3u^2 - 2u^3 of the way, u from 0 to 1 (x256) over the rise's frames
	int u = (frame - ARRIVAL_RISE_FROM) << (8 - ARRIVAL_RISE_SHIFT);

	if (u <= 0)
		return 0;
	if (u >= 256)
		return ARRIVAL_RISE_FULL;
	return (u * u * (768 - 2 * u)) >> 16;
}

const char *arrival_key_name(int key)
{
	if (key < 0)
		key = 0;
	if (key >= ARRIVAL_KEYS)
		key = ARRIVAL_KEYS - 1;
	return key_names[key];
}
