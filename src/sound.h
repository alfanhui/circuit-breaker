#ifndef __SOUND_H__
#define __SOUND_H__

/*
 * Sound effects: raw PCM files on the CD (made by scripts/make_sounds.py), loaded once at
 * start-up and played through the Saturn's sound chip (the SCSP) with jo's audio module.
 */

// Loads the sounds. Once, at start-up, after jo_core_init
void sound_init(void);

// The prologue's hits on the loop (game/loop_break.h): press 1, 2 or 3 (3: it breaks)
void sound_play_hit(int press);

#endif
