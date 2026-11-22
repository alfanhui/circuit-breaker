// Nearest first (game/near_first.h): every index once, by bucket, ties in their own order
#include "test.h"
#include "../src/game/near_first.h"

// Every index comes out exactly once, and the buckets never go back towards you
static void each_once_nearest_first(void)
{
	int distance[NEAR_FIRST_MAX];
	unsigned char order[NEAR_FIRST_MAX];
	unsigned char seen[NEAR_FIRST_MAX] = {0};
	unsigned int r = 12345;

	for (int i = 0; i < NEAR_FIRST_MAX; ++i)
	{
		r = r * 1103515245u + 12345u;
		distance[i] = (int)((r >> 8) % 20000u) - 500; // some negative (as 0), some past the last bucket
	}
	near_first_order(distance, NEAR_FIRST_MAX, order);
	for (int k = 0; k < NEAR_FIRST_MAX; ++k)
	{
		CHECK(order[k] < NEAR_FIRST_MAX);
		++seen[order[k]];
	}
	for (int i = 0; i < NEAR_FIRST_MAX; ++i)
		CHECK_EQ(1, seen[i]);
	for (int k = 1; k < NEAR_FIRST_MAX; ++k)
	{
		const int a = distance[order[k - 1]] < 0 ? 0 : distance[order[k - 1]];
		const int b = distance[order[k]] < 0 ? 0 : distance[order[k]];
		const int bucket_a = (a >> NEAR_FIRST_SHIFT) < NEAR_FIRST_BUCKETS ? a >> NEAR_FIRST_SHIFT : NEAR_FIRST_BUCKETS - 1;
		const int bucket_b = (b >> NEAR_FIRST_SHIFT) < NEAR_FIRST_BUCKETS ? b >> NEAR_FIRST_SHIFT : NEAR_FIRST_BUCKETS - 1;

		CHECK(bucket_a <= bucket_b);
	}
}

// The lift's case: the list's last things are the nearest, and they come out first; ties keep order
static void the_end_of_the_list_first_when_it_is_nearest(void)
{
	const int distance[6] = {9000, 8000, 3000, 3050, 100, 200};
	unsigned char order[6];

	near_first_order(distance, 6, order);
	CHECK_EQ(4, order[0]); // 100 and 200: one bucket, in their own order
	CHECK_EQ(5, order[1]);
	CHECK_EQ(2, order[2]); // 3000 and 3050: one bucket (2560 .. 3071)
	CHECK_EQ(3, order[3]);
	CHECK_EQ(1, order[4]);
	CHECK_EQ(0, order[5]);
}

void run_near_first_tests(void)
{
	RUN(each_once_nearest_first);
	RUN(the_end_of_the_list_first_when_it_is_nearest);
}
