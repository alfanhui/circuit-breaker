#include "camera_shake.h"

void camera_shake_init(camera_shake *s)
{
	s->strength = 0;
	s->frame = 0;
}

void camera_shake_start(camera_shake *s, int strength)
{
	if (strength > s->strength)
		s->strength = strength;
	s->frame = 0; // a fresh jolt starts on the + side
}

void camera_shake_step(camera_shake *s, int *yaw, int *pitch)
{
	int size = s->strength;

	if (size == 0)
	{
		*yaw = 0;
		*pitch = 0;
		return;
	}
	// Bit 0 of the frame flips the yaw every frame; bit 1 flips the pitch every other frame
	*yaw = (s->frame & 1) ? -size : size;
	*pitch = (s->frame & 2) ? -(size >> 1) : size >> 1;
	++s->frame;

	// Die away: keep DECAY sixteenths, and stop once it's too small to see
	size = (size * CAMERA_SHAKE_DECAY) >> 4;
	s->strength = size < CAMERA_SHAKE_MIN ? 0 : size;
}

int camera_shake_active(const camera_shake *s)
{
	return s->strength != 0;
}
