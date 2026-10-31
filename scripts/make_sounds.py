"""
Makes the game's sound effects: raw PCM files in cd/, which jo plays with jo_audio_load_pcm.

    python scripts/make_sounds.py

Only Python's own library: each sound is worked out sample by sample, so it can be tweaked here
and made again. Previews (.wav, to listen to on a PC) go to tests/build/sounds/.

The sound: dark, electric synthwave, like Com Truise. An analog synth, built the way one works:
  - oscillators: sawtooth waves (bright, buzzy: every harmonic), three per note, each a few
    cents out of tune with the others, so they beat against each other: the warm, wide sound
  - a resonant low-pass filter: lets the low harmonics through and cuts the high ones. Its
    cut-off is swept: snapped open (bright) on the hit, then closing (dark). The resonance makes
    it ring and squelch at the cut-off: the "electric" part
  - an amplitude envelope: how loud over time (a quick attack, then dying away)
  - a slow pitch wobble, like a worn tape, and an echo for space

Format (what jo's JoSoundMono16Bit wants): signed 16-bit samples, big-endian (the SH-2's byte
order), one channel, at RATE samples a second. sound.c sets the chip's pitch to match.
"""

import math
import os
import random
import struct
import wave

RATE = 22050  # samples a second: half of 44100, so the chip plays it an octave down (sound.c)


def note(name):
    """A note's frequency: 'A4' is 440 Hz, each semitone is 2^(1/12) of the last."""
    names = {"C": -9, "C#": -8, "D": -7, "D#": -6, "E": -5, "F": -4, "F#": -3, "G": -2, "G#": -1, "A": 0, "A#": 1, "B": 2}
    pitch, octave = name[:-1], int(name[-1])
    return 440.0 * 2 ** ((names[pitch] + 12 * (octave - 4)) / 12)


class Saw:
    """A sawtooth wave, with its corner smoothed (PolyBLEP) so it doesn't alias into a hiss at this
    low sample rate: a naive saw's jump makes harmonics above what 22050 samples a second can hold"""

    def __init__(self, freq, phase=0.0):
        self.freq = freq
        self.phase = phase

    def next(self, bend=1.0):
        step = self.freq * bend / RATE
        self.phase += step
        if self.phase >= 1.0:
            self.phase -= 1.0
        value = 2.0 * self.phase - 1.0
        # PolyBLEP: round off the drop from +1 to -1 over one sample either side of it
        t = self.phase
        if t < step:
            t /= step
            value -= t + t - t * t - 1.0
        elif t > 1.0 - step:
            t = (t - 1.0) / step
            value -= t * t + t + t + 1.0
        return value


class LowPass:
    """A resonant low-pass filter (a state-variable filter, as in many analog synths): cutoff in
    Hz, resonance 0 (none) to ~0.95 (rings at the cutoff)"""

    def __init__(self):
        self.low = 0.0
        self.band = 0.0

    def next(self, x, cutoff, resonance):
        f = 2.0 * math.sin(math.pi * min(cutoff, RATE / 6) / RATE)
        q = 1.0 - resonance
        self.low += f * self.band
        high = x - self.low - q * self.band
        self.band += f * high
        return self.low


def stab(chord, length, cutoff_peak, cutoff_floor, resonance, sweep, decay, detune_cents=9):
    """A synth stab: every note of chord as three detuned saws, through a resonant filter whose
    cutoff jumps to cutoff_peak and falls to cutoff_floor (sweep: how fast), dying away (decay)."""
    rng = random.Random(1)
    voices = []
    for freq in chord:
        for cents in (-detune_cents, 0, detune_cents):
            voices.append(Saw(freq * 2 ** (cents / 1200), rng.random()))
    lpf = LowPass()
    out = []
    for i in range(int(RATE * length)):
        t = i / RATE
        wobble = 1.0 + 0.004 * math.sin(2 * math.pi * 5.5 * t)  # tape wobble: +-7 cents at 5.5 Hz
        x = sum(v.next(wobble) for v in voices) / len(voices)
        cutoff = cutoff_floor + (cutoff_peak - cutoff_floor) * math.exp(-t * sweep)
        y = lpf.next(x, cutoff, resonance)
        attack = min(1.0, t / 0.003)
        out.append(y * attack * math.exp(-t * decay))
    return out


def zap(start, end, length, loud):
    """An electric zap: a square wave whose pitch dives from start to end (Hz), like a discharge."""
    out = []
    phase = 0.0
    for i in range(int(RATE * length)):
        t = i / RATE
        freq = end + (start - end) * math.exp(-t * 18.0)
        phase = (phase + freq / RATE) % 1.0
        out.append((1.0 if phase < 0.5 else -1.0) * loud * math.exp(-t * 9.0))
    return out


def crackle(length, loud):
    """Static: noise in short random bursts, dying away: sparks off the break."""
    rng = random.Random(7)
    out = []
    lpf = LowPass()
    burst = 0
    for i in range(int(RATE * length)):
        t = i / RATE
        if burst <= 0 and rng.random() < 0.004:
            burst = rng.randint(40, 300)  # a spark: a few ms of noise
        x = rng.uniform(-1, 1) if burst > 0 else 0.0
        burst -= 1
        out.append(lpf.next(x, 5000, 0.3) * loud * math.exp(-t * 3.0))
    return out


def mix(*parts):
    length = max(len(p) for p in parts)
    return [sum(p[i] for p in parts if i < len(p)) for i in range(length)]


def echo(samples, delay, feedback, wet, tail):
    """A dub echo: each repeat delay seconds later and feedback as loud, darkened a little each
    time (a one-pole low-pass in the loop), mixed in at wet. tail: seconds added for the echoes."""
    d = int(RATE * delay)
    out = list(samples) + [0.0] * int(RATE * tail)
    buffer = [0.0] * d
    dark = 0.0
    for i in range(len(out)):
        delayed = buffer[i % d]
        dark += 0.35 * (delayed - dark)
        buffer[i % d] = out[i] + dark * feedback
        out[i] += dark * wet
    return out


def write(name, samples, volume):
    """Its loudest sample made volume (1.0: as loud as 16 bits allow, short of clipping): the
    game's 16-bit big-endian file in cd/, and a .wav preview in tests/build/sounds/."""
    peak = max(abs(s) for s in samples) or 1.0
    values = [int(s / peak * volume * 32000) for s in samples]
    here = os.path.dirname(__file__)
    with open(os.path.join(here, "..", "cd", name + ".PCM"), "wb") as f:
        f.write(b"".join(struct.pack(">h", v) for v in values))
    previews = os.path.join(here, "..", "tests", "build", "sounds")
    os.makedirs(previews, exist_ok=True)
    with wave.open(os.path.join(previews, name + ".wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", v) for v in values))  # WAV is little-endian
    print(f"{name}: {len(samples) / RATE:.2f} s, {len(values) * 2} bytes")


if __name__ == "__main__":
    # The first press: a low D minor stab (D, F, A), the filter snapping open and shut
    hit1 = stab([note("D3"), note("F3"), note("A3")], 0.45, 2600, 300, 0.55, 14, 6.5)
    write("HIT1", echo(hit1, 0.16, 0.35, 0.4, 0.35), 0.6)

    # The second: the loop strains. A diminished stab, tenser (F, G#, B over D), more resonance
    hit2 = stab([note("D3"), note("F3"), note("G#3"), note("B3")], 0.5, 3400, 260, 0.75, 11, 5.5)
    hit2 = mix(hit2, zap(900, 120, 0.25, 0.12))
    write("HIT2", echo(hit2, 0.16, 0.4, 0.45, 0.4), 0.8)

    # The third: it breaks. A heavy D minor chord over an octave of bass, the filter wide open and
    # slow to close, a falling zap and a crackle of sparks; a longer echo
    hit3 = stab([note("D2"), note("D3"), note("A3"), note("D4"), note("F4")], 1.0, 5000, 220, 0.8, 4.5, 2.6, 12)
    hit3 = mix(hit3, zap(1600, 60, 0.6, 0.2), crackle(1.0, 0.25))
    write("HIT3", echo(hit3, 0.21, 0.45, 0.5, 0.5), 1.0)
