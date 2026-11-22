#include "test.h"
#include "../src/utils/fastmath.h"

/*
 * The quick forms (src/utils/fastmath.h) give what plain C does. ASR against >> (on the PC, as on
 * GCC for the SH-2, >> of a negative int shifts arithmetically): the ends of the range, round
 * numbers either side of 0, and a sweep with odd steps, for every shift count the code uses
 */
static void asr_is_the_arithmetic_shift(void)
{
	static const int edges[] = {0, 1, -1, 2, -2, 1023, 1024, 1025, -1023, -1024, -1025, 0x7FFFFFFF, -0x7FFFFFFF - 1,
								0x40000000, -0x40000000, 123456789, -123456789};
	int checked = 0;

	for (int n = 1; n <= 20; ++n)
	{
		for (int k = 0; k < (int)(sizeof(edges) / sizeof(edges[0])); ++k)
		{
			CHECK_EQ(edges[k] >> n, ASR(edges[k], n));
			++checked;
		}
		for (long long x = -0x7FFFFFFFLL - 1; x <= 0x7FFFFFFFLL; x += 104729LL * 97)
		{
			CHECK_EQ((int)x >> n, ASR((int)x, n));
			++checked;
		}
	}
	CHECK(checked > 20 * 400);
}

// A product shifted down, as the code writes it: ASR(a * b, 10)
static void asr_of_a_product(void)
{
	for (int a = -3000; a <= 3000; a += 37)
		for (int b = -1024; b <= 1024; b += 61)
			CHECK_EQ((a * b) >> 10, ASR(a * b, 10));
}

static void bit_set_reads_each_bit(void)
{
	for (int byte = 0; byte < 256; ++byte)
		for (int bit = 0; bit < 8; ++bit)
			CHECK_EQ(((byte >> bit) & 1) != 0, BIT_SET(byte, bit));
}

static void fast_udiv_divides(void)
{
	CHECK_EQ(7u, fast_udiv(7u << 16, 1u << 16));
	CHECK_EQ(0u, fast_udiv(5u, 6u));
	CHECK_EQ(32767u, fast_udiv(0x7FFF0000u, 0x10000u));
}

void run_fastmath_tests(void)
{
	RUN(asr_is_the_arithmetic_shift);
	RUN(asr_of_a_product);
	RUN(bit_set_reads_each_bit);
	RUN(fast_udiv_divides);
}
