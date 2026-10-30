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
void run_turn_ease_tests(void);
void run_depth_fog_tests(void);
void run_turn_gate_tests(void);
void run_collision_tests(void);
void run_minimap_tests(void);
void run_rider_tests(void);
void run_bot_tests(void);
void run_trail_occlusion_tests(void);
void run_game_state_tests(void);

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
	run_turn_ease_tests();
	run_depth_fog_tests();
	run_turn_gate_tests();
	run_collision_tests();
	run_minimap_tests();
	run_rider_tests();
	run_bot_tests();
	run_trail_occlusion_tests();
	run_game_state_tests();

	printf("\n%d checks, %d failed\n", test_checks, test_failures);
	return test_failures == 0 ? 0 : 1;
}
