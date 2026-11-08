#include <jo/jo.h>
#include "sound.h"

/*
 * How fast the sound chip (the SCSP) plays them. jo calls this sample_rate and its docs say Hz,
 * but SGL hands it to the chip as is, and the chip wants its own pitch word:
 *     bits 14-11  OCT, an octave, -8..7 (4 bits, two's complement)
 *     bits  9-0   FNS, a fine step, 0..1023
 * which plays 44100 x 2^OCT x (1 + FNS / 1024) samples a second. (Left at 0, it's 44100: that's
 * why jo's sound worked in the Shenmue project, which never set it.)
 * scripts/make_sounds.py makes them at 22050, half of 44100: an octave down, OCT -1 (binary
 * 1111) and FNS 0, so 0xF << 11. Given 22050 itself (0x5622: OCT -6, FNS 546), the chip played
 * ~1050 samples a second, 20 times too slow: a 0.45 s ping became a long, low smear
 */
#define SOUND_PITCH_22050 (0xF << 11)

static jo_sound hits[3];
static const char *const hit_files[3] = {"HIT1.PCM", "HIT2.PCM", "HIT3.PCM"};

void sound_init(void)
{
	for (int i = 0; i < 3; ++i)
	{
		// 16-bit mono, big-endian: as make_sounds.py writes them
		jo_audio_load_pcm(hit_files[i], JoSoundMono16Bit, &hits[i]);
		hits[i].sample_rate = SOUND_PITCH_22050;
	}
}

void sound_play_hit(int press)
{
	if (press >= 1 && press <= 3)
		jo_audio_play_sound(&hits[press - 1]);
}
