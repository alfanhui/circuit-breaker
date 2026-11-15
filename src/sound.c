#include <jo/jo.h>
#include "sound.h"
// (SFX_ON, features.h, is a pause-menu switch: input.c pauses the sound while it is off)

/*
 * How fast the sound chip (the SCSP) plays them: scripts/make_sounds.py makes them at 22050
 * samples a second. Since jo-engine 2026, sample_rate is in Hz, as its docs say: jo works out the
 * chip's own pitch word from it (audio.c, __jo_scsp_pitch: an octave and a fine step).
 * Before, on SGL, the word went to the chip as it was, so this was the word itself, 0xF << 11
 * (an octave down from 44100). Left at that after the move, jo read it as 30720 Hz: every sound
 * played 1.39 times too fast, 6 semitones high (found adding the turn's whoosh, 2026-11-13)
 */
#define SOUND_RATE 22050

static jo_sound hits[3];
static const char *const hit_files[3] = {"HIT1.PCM", "HIT2.PCM", "HIT3.PCM"};
/*
 * The turn's whoosh, twice: the same samples (loaded once, one copy in memory), each with its own
 * pan, so a left turn's heard on the left and a right turn's on the right. Not all the way over:
 * the other ear hears it too, as air rushing past would be. jo turns the pan into the SCSP's
 * level for the other side, pan / 8 steps of 3 dB down (audio.c, jo_audio_pan): 24 is 3 steps,
 * -9 dB, a third as loud. (80, 10 steps, was 30 times quieter there: it sounded only in one ear)
 */
#define WHOOSH_PAN 24
static jo_sound whoosh_left, whoosh_right;

/*
 * The motor's loop (HUM.PCM, make_sounds.py) plays round and round on a channel of its own, the
 * last one: jo gives the other sounds the first free channel, from 0. Its pitch is how fast the
 * chip plays it (jo_audio_set_channel_sample_rate, in Hz: no new sound, one register).
 * It's an electric car's warning sound (AVAS), after a recording the user chose (make_sounds.py
 * has what was measured): a soft chord whose root glides from 334 Hz stood still to 398 Hz at
 * speed, only 19% in all. The first try was a motor's whine, 600 to 840 Hz, 40% up on a boost:
 * "too high pitch and annoying" (the user, 2026-11-14). Made at 11025 with its root at 366 Hz:
 * played at 10050 it's 334 (setting off), 11025 at cruise, 12000 at full boost (398). The rate
 * eases to where it's wanted, a step a frame: it glides, as the recording does (0.8 s from
 * setting off to cruise). Multiplies and adds only
 */
#define ENGINE_CHANNEL (JO_SOUND_MAX_CHANNEL - 1)
#define ENGINE_RATE_START 10050 // setting off: the root at 334 Hz
#define ENGINE_RATE_CRUISE 11025
#define ENGINE_RATE_BOOST 61 // more for each level of boost (16 of them: 12000 at full, 398 Hz)
#define ENGINE_RATE_STEP 20	 // a frame
// Its loudness, of 127. jo's volume is the SCSP's attenuation, ~0.8 dB a step down from 127 (not
// a share: at 70 it was 45 dB down, all but silent, measured from a recording). 98: 23 dB down,
// a quarter as loud as the hits, as it's on all the time
#define ENGINE_VOLUME 98
static jo_sound engine;
static bool engine_on = false;
static int engine_rate;
static bool engine_resume = false; // stopped by a pause: it comes back at engine_rate

static bool paused = false; // sound_pause

void sound_init(void)
{
	// All loaded, even if they start switched off (SFX_ON): the pause menu can turn them on
	for (int i = 0; i < 3; ++i)
	{
		// 16-bit mono, big-endian: as make_sounds.py writes them
		jo_audio_load_pcm(hit_files[i], JoSoundMono16Bit, &hits[i]);
		hits[i].sample_rate = SOUND_RATE;
	}
	jo_audio_load_pcm("WHOOSH.PCM", JoSoundMono16Bit, &whoosh_left);
	whoosh_left.sample_rate = SOUND_RATE;
	whoosh_right = whoosh_left; // the same data, another pan
	whoosh_left.pan = -WHOOSH_PAN;
	whoosh_right.pan = WHOOSH_PAN;
	jo_audio_load_pcm("HUM.PCM", JoSoundMono16Bit, &engine);
	engine.volume = ENGINE_VOLUME;
}

void sound_engine(bool riding, int boost)
{
	const int wanted = ENGINE_RATE_CRUISE + boost * ENGINE_RATE_BOOST;

	if (paused)
		return;
	if (!riding)
	{
		engine_resume = false; // the ride's over: the next one winds up from a standstill
		if (engine_on)
			jo_audio_stop_sound(&engine);
		engine_on = false;
		return;
	}
	if (!engine_on)
	{
		if (!engine_resume)
			engine_rate = ENGINE_RATE_START;
		engine_resume = false;
		engine.sample_rate = (unsigned short)engine_rate;
		jo_audio_play_sound_loop_on_channel(&engine, ENGINE_CHANNEL);
		engine_on = true;
		return;
	}
	if (engine_rate == wanted)
		return;
	engine_rate = engine_rate < wanted - ENGINE_RATE_STEP	? engine_rate + ENGINE_RATE_STEP
				  : engine_rate > wanted + ENGINE_RATE_STEP ? engine_rate - ENGINE_RATE_STEP
															: wanted;
	jo_audio_set_channel_sample_rate(ENGINE_CHANNEL, (unsigned short)engine_rate);
}

void sound_pause(bool now)
{
	if (now == paused)
		return;
	paused = now;
	if (!paused)
		return;
	// Everything that may be playing, stopped (jo finds each one's channel, if it's still on one)
	for (int i = 0; i < 3; ++i)
		jo_audio_stop_sound(&hits[i]);
	jo_audio_stop_sound(&whoosh_left);
	jo_audio_stop_sound(&whoosh_right);
	if (engine_on)
	{
		jo_audio_stop_sound(&engine);
		engine_on = false;
		engine_resume = true;
	}
}

void sound_play_turn(int direction)
{
	if (paused)
		return;
	// jo plays it on a free channel: a quick second turn overlaps the first's tail
	jo_audio_play_sound(direction < 0 ? &whoosh_left : &whoosh_right);
}

void sound_play_hit(int press)
{
	if (paused)
		return;
	if (press >= 1 && press <= 3)
		jo_audio_play_sound(&hits[press - 1]);
}
