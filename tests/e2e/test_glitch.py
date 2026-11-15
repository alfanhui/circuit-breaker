"""Unit tests for glitch.py (SG-6's checks), on made-up frames: run with
    python tests/e2e/test_glitch.py
The real proof is the film, flash_look_up.steps; these pin down each rule on its own."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glitch  # noqa: E402

W, H = 64, 48


def picture(background, rects=()):
    """A W x H frame of one colour, with (x, y, w, h, colour) rectangles drawn over it."""
    d = bytearray(bytes(background) * (W * H))
    for x0, y0, w, h, colour in rects:
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                d[(y * W + x) * 3:(y * W + x) * 3 + 3] = bytes(colour)
    return W, H, bytes(d)


class Frames:
    """Stands in for glitch.Recording: a list of frames."""

    def __init__(self, frames):
        self.frames = frames

    def __len__(self):
        return len(self.frames)

    def frame(self, i):
        return self.frames[i]


GREY, WHITE, PINK, MAGENTA = (60, 60, 70), (230, 230, 230), (240, 112, 160), (255, 0, 255)


def moving_bar(steps, stride=6):
    """A white bar going right across a grey screen, still for 3 frames at each end."""
    frames = [picture(GREY, [(0, 10, 16, 20, WHITE)])] * 3
    frames += [picture(GREY, [(i * stride, 10, 16, 20, WHITE)]) for i in range(1, steps)]
    return frames + [frames[-1]] * 3


class TestGlitch(unittest.TestCase):
    def setUp(self):
        # Made-up frames are small: the thresholds scaled to them (the film's are ~84,000 samples)
        self.saved = glitch.STILL_CHANGE, glitch.FLASH_MIN, glitch.STRAY_MIN, glitch.HOLE_MIN
        glitch.STILL_CHANGE, glitch.FLASH_MIN, glitch.STRAY_MIN, glitch.HOLE_MIN = 10, 20, 10, 5

    def tearDown(self):
        glitch.STILL_CHANGE, glitch.FLASH_MIN, glitch.STRAY_MIN, glitch.HOLE_MIN = self.saved

    def test_a_png_reads_back_as_written(self):
        frame = picture(GREY, [(5, 5, 10, 10, PINK)])
        self.assertEqual(glitch.decode_png(glitch.encode_png(*frame)), frame)

    def test_frames_are_found_in_a_movie_by_their_signature(self):
        a, b = picture(GREY), picture(WHITE)
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"moov header" + glitch.encode_png(*a) + b"between" + glitch.encode_png(*b) + b"\x89PNG half")
        rec = glitch.Recording(f.name)
        self.assertEqual((len(rec), rec.frame(0), rec.frame(1)), (2, a, b))
        os.unlink(f.name)

    def test_a_smooth_move_is_neither_a_flash_nor_stray(self):
        frames = Frames(moving_bar(6))
        still = glitch.stills(frames, 0, len(frames))
        self.assertEqual(glitch.flash_frames(frames, 0, len(frames)), [])
        self.assertEqual(glitch.stray_frames(frames, 0, len(frames), still), [])

    def test_a_frame_unlike_both_neighbours_is_a_flash(self):
        frames = moving_bar(6, stride=2)
        frames[5] = picture(GREY, [(0, 0, W, H // 2, WHITE)])  # half the screen white for a frame
        found = glitch.flash_frames(Frames(frames), 0, len(frames))
        self.assertEqual([f[0] for f in found], [5])

    def test_a_frame_shown_twice_is_one_picture(self):
        # The game at 30 fps: each picture twice. Not a flash, and its repeats aren't faults
        frames = [f for f in moving_bar(6) for _ in (0, 1)]
        self.assertEqual(glitch.flash_frames(Frames(frames), 0, len(frames)), [])

    def test_a_colour_no_still_frame_has_is_stray(self):
        frames = moving_bar(6)
        frames[5] = picture(GREY, [(5 * 6, 10, 16, 20, WHITE), (0, 40, W, 4, PINK)])  # a pink band
        frames = Frames(frames)
        found = glitch.stray_frames(frames, 0, len(frames), glitch.stills(frames, 0, len(frames)))
        self.assertEqual([(f[0], f[2]) for f in found], [(5, (240, 112, 160))])

    def test_a_colour_one_step_from_a_still_one_is_not_stray(self):
        # Fog shades a little apart from the still frames' are the same colour
        frames = moving_bar(6)
        frames[5] = picture((72, 60, 70), [(5 * 6, 10, 16, 20, WHITE)])
        frames = Frames(frames)
        self.assertEqual(glitch.stray_frames(frames, 0, len(frames), glitch.stills(frames, 0, len(frames))), [])

    def test_the_magenta_back_colour_is_a_hole(self):
        frames = [picture(GREY), picture(GREY, [(10, 30, 20, 6, MAGENTA)]), picture(GREY)]
        self.assertEqual(glitch.hole_frames(Frames(frames), 0, 3), [(1, 30, 30, 34)])


if __name__ == "__main__":
    unittest.main()
