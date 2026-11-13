#include "test.h"
#include "../src/game/landmarks.h"

// Twice a shape's area, signed by the way round its corners go (the shoelace sum)
static int area2(const landmark_shape *s)
{
	int sum = 0;

	for (int k = 0; k < 4; ++k)
	{
		const int n = (k + 1) & 3;

		sum += s->along[k] * s->up[n] - s->along[n] * s->up[k];
	}
	return sum;
}

// Whatever the frame, every shape stays inside its landmark's width and height (level.c leaves a
// landmark out by that box), is a real polygon (never flat, never turned inside out), and there
// are never more than there's room for
static void every_shape_stays_in_its_box(void)
{
	for (int i = 0; i < LANDMARK_COUNT; ++i)
		for (unsigned int frame = 0; frame < 2048; frame += 7)
		{
			landmark_shape shapes[LANDMARK_SHAPES_MAX];
			const int count = landmark_shapes(i, frame, shapes);

			CHECK(count >= 1 && count <= LANDMARK_SHAPES_MAX);
			for (int s = 0; s < count; ++s)
			{
				CHECK(area2(&shapes[s]) > 100 * 100);
				for (int k = 0; k < 4; ++k)
				{
					CHECK(shapes[s].along[k] >= -landmarks[i].half_width && shapes[s].along[k] <= landmarks[i].half_width);
					CHECK(shapes[s].up[k] >= 0 && shapes[s].up[k] <= landmarks[i].height);
				}
				CHECK(shapes[s].colour & 0x8000); // an RGB colour: without the top bit VDP1 reads a palette
			}
		}
}

// A world coordinate has to fit jo's 16.16 (under 32 768), ends and all, and each stands outside
// the largest arena (14 000 from the middle to a wall) but the tower at its middle
static void they_stand_far_off_and_fit_the_world(void)
{
	for (int i = 0; i < LANDMARK_COUNT; ++i)
	{
		const landmark *l = &landmarks[i];
		const int out = l->out < 0 ? -l->out : l->out, across = l->across < 0 ? -l->across : l->across;

		CHECK(out + (l->faces_out ? 0 : l->half_width) < 30000);
		CHECK(across + (l->faces_out ? l->half_width : 0) < 30000);
		if (l->place != PLACE_ARENA)
			CHECK(out > 16000 || across > 16000);
	}
}

// One for every place but the CD block (the disc over its hub) and Work RAM (the skyline all
// round), and no two the same
static void one_for_each_place(void)
{
	unsigned seen = 0;

	for (int i = 0; i < LANDMARK_COUNT; ++i)
	{
		CHECK(landmarks[i].place != PLACE_CD_BLOCK && landmarks[i].place != PLACE_WORK_RAM);
		CHECK((seen & (1u << landmarks[i].place)) == 0);
		seen |= 1u << landmarks[i].place;
	}
	CHECK_EQ(0xFFu & ~(1u << PLACE_CD_BLOCK) & ~(1u << PLACE_WORK_RAM), seen);
}

// What moves does move, and comes round again: VDP1's fill grows and starts over
static void vdp1_fills_and_starts_again(void)
{
	landmark_shape early[LANDMARK_SHAPES_MAX], late[LANDMARK_SHAPES_MAX], again[LANDMARK_SHAPES_MAX];
	int which = 0;

	while (landmarks[which].place != PLACE_VDP1)
		++which;
	landmark_shapes(which, 40, early);
	landmark_shapes(which, 500, late);
	landmark_shapes(which, 40 + 512, again);
	CHECK(area2(&late[1]) > 4 * area2(&early[1]));
	CHECK_EQ(area2(&early[1]), area2(&again[1]));
	CHECK(area2(&late[1]) <= area2(&late[0])); // never past the polygon it's filling
}

void run_landmarks_tests(void)
{
	RUN(every_shape_stays_in_its_box);
	RUN(they_stand_far_off_and_fit_the_world);
	RUN(one_for_each_place);
	RUN(vdp1_fills_and_starts_again);
}
