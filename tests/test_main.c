#include "test.h"

int test_failures = 0;
int test_checks = 0;

void run_trail_tests(void);
void run_cycle_tests(void);

int main(void)
{
	run_trail_tests();
	run_cycle_tests();

	printf("\n%d checks, %d failed\n", test_checks, test_failures);
	return test_failures == 0 ? 0 : 1;
}
