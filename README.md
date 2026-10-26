# Circuit Breaker

Sega Saturn homebrew game built with jo-engine.

## How to run

1. Place within a folder under jo-engine's 'Projects' folder.
2. Compile and run the game: `./run.sh`

## Tests

Game logic that doesn't depend on the Saturn (`src/game/`: cycle movement and light-cycle trails) is plain C and unit-tested on the PC:

- Windows: `tests\run_tests.bat` (uses gcc if it's on PATH, otherwise Visual Studio's `cl`)
- Linux/macOS: `./tests/run_tests.sh`

## Controls

- Left / Right: turn 90°
- Up: boost
- Down: switch between isometric and 1st person view
- A: clear the trail
- Start: debug camera (D-pad pans, L/R height, B/C pitch)
