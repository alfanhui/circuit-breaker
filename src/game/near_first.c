#include "near_first.h"

// A distance's bucket: its 512s, the last bucket for everything further
static int bucket_of(int distance)
{
	const int b = distance <= 0 ? 0 : distance >> NEAR_FIRST_SHIFT;

	return b < NEAR_FIRST_BUCKETS ? b : NEAR_FIRST_BUCKETS - 1;
}

void near_first_order(const int *distance, int count, unsigned char *order)
{
	// starts[b]: where bucket b's things begin in the order. First how many fall in each bucket...
	unsigned char starts[NEAR_FIRST_BUCKETS];
	int total = 0;

	for (int b = 0; b < NEAR_FIRST_BUCKETS; ++b)
		starts[b] = 0;
	for (int i = 0; i < count; ++i)
		++starts[bucket_of(distance[i])];
	// ...then each bucket starts where the ones nearer than it end (a running total)
	for (int b = 0; b < NEAR_FIRST_BUCKETS; ++b)
	{
		const int in_it = starts[b];

		starts[b] = (unsigned char)total;
		total += in_it;
	}
	// Each thing into the next free place in its bucket: in their own order within it (stable)
	for (int i = 0; i < count; ++i)
		order[starts[bucket_of(distance[i])]++] = (unsigned char)i;
}
