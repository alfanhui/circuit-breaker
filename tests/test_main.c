#include "test.h"

int test_failures = 0;
int test_checks = 0;

void run_trail_tests(void);
void run_cycle_tests(void);
void run_press_latch_tests(void);
void run_arena_tests(void);
void run_trail_pieces_tests(void);
void run_floor_fade_tests(void);
void run_view_cull_tests(void);
void run_wall_clip_tests(void);

int main(void)
{
	run_trail_tests();
	run_cycle_tests();
	run_press_latch_tests();
	run_arena_tests();
	run_trail_pieces_tests();
	run_floor_fade_tests();
	run_view_cull_tests();
	run_wall_clip_tests();

	printf("\n%d checks, %d failed\n", test_checks, test_failures);
	return test_failures == 0 ? 0 : 1;
}
