#ifndef __TEST_H__
#define __TEST_H__

// Minimal host-side test harness for the pure game modules in src/game.

#include <stdio.h>

extern int test_failures;
extern int test_checks;

#define CHECK(cond)                                                     \
	do                                                                  \
	{                                                                   \
		test_checks++;                                                  \
		if (!(cond))                                                    \
		{                                                               \
			test_failures++;                                            \
			printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
		}                                                               \
	} while (0)

#define CHECK_EQ(expected, actual)                                                               \
	do                                                                                           \
	{                                                                                            \
		long e_ = (long)(expected), a_ = (long)(actual);                                         \
		test_checks++;                                                                           \
		if (e_ != a_)                                                                            \
		{                                                                                        \
			test_failures++;                                                                     \
			printf("  FAIL %s:%d: %s == %s (expected %ld, got %ld)\n", __FILE__, __LINE__,      \
				   #expected, #actual, e_, a_);                                                  \
		}                                                                                        \
	} while (0)

#define RUN(test)                \
	do                           \
	{                            \
		printf("%s\n", #test);   \
		test();                  \
	} while (0)

#endif
