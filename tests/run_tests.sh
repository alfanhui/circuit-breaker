#!/bin/bash
# Builds and runs the host unit tests for the pure game modules (src/game).
set -e
cd "$(dirname "$0")"
mkdir -p build
${CC:-gcc} -std=c99 -W -Wall -Wextra -Werror -o build/tests \
	test_main.c test_trail.c test_cycle.c test_press_latch.c ../src/game/trail.c ../src/game/cycle.c ../src/game/press_latch.c
./build/tests
