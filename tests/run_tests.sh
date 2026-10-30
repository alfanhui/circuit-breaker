#!/bin/bash
# Builds and runs the host unit tests for the pure game modules (src/game).
set -e
cd "$(dirname "$0")"
mkdir -p build
${CC:-gcc} -std=c99 -W -Wall -Wextra -Werror -o build/tests \
	test_main.c test_trail.c test_cycle.c test_press_latch.c test_arena.c test_trail_pieces.c test_floor_fade.c test_view_cull.c test_wall_clip.c test_turn_ease.c test_depth_fog.c test_turn_gate.c test_collision.c test_minimap.c test_rider.c test_bot.c test_trail_occlusion.c test_game_state.c ../src/game/trail.c ../src/game/cycle.c ../src/game/press_latch.c ../src/game/arena.c ../src/game/trail_pieces.c ../src/game/floor_fade.c ../src/game/view_cull.c ../src/game/wall_clip.c ../src/game/turn_ease.c ../src/game/depth_fog.c ../src/game/turn_gate.c ../src/game/collision.c ../src/game/minimap.c ../src/game/rider.c ../src/game/bot.c ../src/game/trail_occlusion.c ../src/game/game_state.c
./build/tests
