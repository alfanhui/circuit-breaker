#ifndef __SOUND_H__
#define __SOUND_H__

#include <stdbool.h>

/*
 * Sound effects: raw PCM files on the CD (made by scripts/make_sounds.py), loaded once at
 * start-up and played through the Saturn's sound chip (the SCSP) with jo's audio module.
 * Switched off (the pause menu's "Sound effects", SFX_ON in features.h at boot), they're silent
 * as if paused: input.c passes sound_pause the two together.
 */

// Loads the sounds. Once, at start-up, after jo_core_init
void sound_init(void);

// The prologue's hits on the loop (game/loop_break.h): press 1, 2 or 3 (3: it breaks)
void sound_play_hit(int press);

// A whoosh as the cycle turns: direction is TURN_LEFT (-1) or TURN_RIGHT (1, game/turn_gate.h),
// and it's heard on that side
void sound_play_turn(int direction);

/*
 * The cycle's motor: an electric hum that sings higher as you boost. Every frame: riding (you're
 * on the cycle, moving, and it's you riding it) and the boost's level (0 .. CYCLE_BOOST_FULL,
 * game/cycle.h). It starts low as you set off and winds up; not riding, it stops
 */
void sound_engine(bool riding, int boost);

/*
 * Every frame: whether the sound's held: the game's paused (START, input.c), or the menu has the
 * sound effects off. Pausing stops every
 * sound playing at once (a hit, a whoosh, the hum), and nothing plays while it's paused: the
 * calls above do nothing. Unpaused, the hum comes back where it was (its pitch kept, not wound
 * up again from a standstill), the next time sound_engine says you're riding
 */
void sound_pause(bool paused);

#endif
