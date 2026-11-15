"""Glitch checks on a film's recording (docs/BACKLOG.md SG-6): finds the faults that last a frame,
which a shot only catches when it happens to land on one.

Mednafen records every frame it shows (-qtrecord, PNG codec: run.sh's "record" step). Each frame
is a complete PNG inside the movie's data, so the frames are found by their PNG signature, even
while Mednafen is still writing (the movie's index is written only at the end). The idea and the
reading are jo-engine's own test harness (tests/harness/media.py, glitch.py), written again here
small: Python's standard library only, so the E2E container needs nothing else.

Commands (run.sh calls them):
  glitch.py count MOVIE                       how many frames are recorded so far
  glitch.py flashes MOVIE FROM TO OUT [NAME]  the flash check on frames FROM..TO-1; exits 1 if any
  glitch.py holes MOVIE FROM TO OUT [NAME]    the holes check (magenta below the horizon); exits 1
"""

import os
import struct
import sys
import zlib

PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


# --- Reading the recording -------------------------------------------------------------------

def frame_offsets(path):
    """(start, end) in the file of each complete PNG frame recorded so far."""
    if not os.path.exists(path):
        return []
    with open(path, "rb") as f:
        data = f.read()
    offsets = []
    pos = 0
    while True:
        start = data.find(PNG_SIGNATURE, pos)
        if start < 0:
            break
        end = data.find(b"IEND", start)
        if end < 0 or end + 8 > len(data):
            break  # still being written
        end += 8  # "IEND" and its CRC
        offsets.append((start, end))
        pos = end
    return offsets


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def decode_png(png):
    """(width, height, rgb bytes) of an 8-bit RGB, RGBA or palette PNG, not interlaced (Mednafen's
    recording is RGB; ImageMagick's shots of the window are palette PNGs)."""
    if png[:8] != PNG_SIGNATURE:
        raise ValueError("not a PNG")
    pos, idat = 8, []
    width = height = 0
    color_type = 2
    palette = b""
    while pos < len(png):
        length, ctype = struct.unpack(">I4s", png[pos:pos + 8])
        body = png[pos + 8:pos + 8 + length]
        if ctype == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if (depth != 8 and not (color_type == 3 and depth in (1, 2, 4))) or color_type not in (2, 3, 6) or interlace:
                raise ValueError("unsupported PNG format")
        elif ctype == b"PLTE":
            palette = body
        elif ctype == b"IDAT":
            idat.append(body)
        elif ctype == b"IEND":
            break
        pos += 12 + length
    bpp = {2: 3, 3: 1, 6: 4}[color_type]
    stride = width * bpp
    if depth < 8:
        stride = (width * depth + 7) // 8  # a palette of 2, 4 or 16 colours: several pixels a byte
    raw = zlib.decompress(b"".join(idat))
    out = bytearray(stride * height)
    prev = bytearray(stride)
    for y in range(height):
        at = y * (stride + 1)
        kind = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        # PNG's filters: each line is stored as a difference from its left, its top, or both
        if kind == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 0xFF
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif kind == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif kind == 4:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                upleft = prev[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + _paeth(left, prev[i], upleft)) & 0xFF
        out[y * stride:(y + 1) * stride] = line
        prev = line
    if bpp == 4:
        del out[3::4]  # drop the alpha
    elif bpp == 1:  # each pixel an index into the palette's RGB triples
        if depth < 8:  # unpacked from several a byte, high bits first
            per = 8 // depth
            mask = (1 << depth) - 1
            idx = bytearray()
            for y in range(height):
                row = out[y * stride:(y + 1) * stride]
                for x in range(width):
                    idx.append((row[x // per] >> (8 - depth * (x % per + 1))) & mask)
            out = idx
        out = bytearray(b"".join(palette[i * 3:i * 3 + 3] for i in out))
    return width, height, bytes(out)


def encode_png(width, height, rgb):
    """An RGB picture as a PNG (no filter: these are only the few frames a check saves)."""
    stride = width * 3
    rows = b"".join(b"\x00" + rgb[y * stride:(y + 1) * stride] for y in range(height))

    def chunk(ctype, body):
        return struct.pack(">I", len(body)) + ctype + body + struct.pack(">I", zlib.crc32(ctype + body) & 0xFFFFFFFF)

    return (PNG_SIGNATURE + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))


class Recording:
    """The frames of a Mednafen recording, decoded on demand and kept (a check reads each a few times)."""

    def __init__(self, path):
        self.path = path
        self.offsets = frame_offsets(path)
        self._decoded = {}

    def __len__(self):
        return len(self.offsets)

    def png(self, i):
        start, end = self.offsets[i]
        with open(self.path, "rb") as f:
            f.seek(start)
            return f.read(end - start)

    def frame(self, i):
        if i not in self._decoded:
            self._decoded[i] = decode_png(self.png(i))
        return self._decoded[i]


# --- Comparing frames ------------------------------------------------------------------------

STEP = 2  # every 2nd pixel of every 2nd line: a quarter of the work, and a fault is never 1 pixel


def changed(a, b, threshold=32):
    """How many of the sampled pixels differ by more than threshold (in any of R, G, B)."""
    wa, ha, da = a
    wb, hb, db = b
    if (wa, ha) != (wb, hb):
        return (wa // STEP) * (ha // STEP)
    if da == db:
        return 0
    n = 0
    for y in range(0, ha, STEP):
        row = y * wa * 3
        for x in range(row, row + wa * 3, STEP * 3):
            if (abs(da[x] - db[x]) > threshold or abs(da[x + 1] - db[x + 1]) > threshold
                    or abs(da[x + 2] - db[x + 2]) > threshold):
                n += 1
    return n


# A frame "still": it changed from the one before in fewer samples than this (~1.2% of the
# screen). The debug lines and the HUD's blinks change ~250 between still frames; a move, thousands
STILL_CHANGE = 1000


def stills(rec, lo, hi):
    """The frames in lo..hi-1 that hardly changed from the one before: nothing on screen moving."""
    return [i for i in range(max(lo, 1), hi) if changed(rec.frame(i - 1), rec.frame(i)) < STILL_CHANGE]


# --- The flash check: a picture that differs from both its neighbours -------------------------
#
# Through a move, each frame differs from the next by a little, and a frame from the one two on by
# more: the change adds up. A frame that's wrong for one frame (a polygon thrown across the screen,
# a missing wall) differs from both its neighbours by far more than they differ from each other.
# Measured through a quick look up (flash_look_up.steps): min(before, after) / across was 0.3-0.64
# for good frames, where a flash is above 1.

FLASH_RATIO = 1.5  # min(change before, change after) above this times the change across: a flash
FLASH_MIN = 1000  # ...and above this many samples, so the HUD's blink on a still view isn't one


def flash_frames(rec, lo, hi):
    """[(frame, before, after, across)] of the one-frame faults in lo..hi-1. A frame shown twice (the
    game at 30 fps, or still) is compared as one picture: repeats are skipped, not counted."""
    pictures = []  # the frames whose picture differs from the one before
    for i in range(lo, hi):
        if not pictures or rec.frame(i)[2] != rec.frame(pictures[-1])[2]:
            pictures.append(i)
    found = []
    for k in range(1, len(pictures) - 1):
        p, i, n = pictures[k - 1], pictures[k], pictures[k + 1]
        before = changed(rec.frame(p), rec.frame(i))
        after = changed(rec.frame(i), rec.frame(n))
        across = changed(rec.frame(p), rec.frame(n))
        if min(before, after) > FLASH_MIN and min(before, after) > FLASH_RATIO * across:
            found.append((i, before, after, across))
    return found


# --- The stray-colour check: a colour a move shows that no still picture has -----------------
#
# The pink band (VDP2's sky a frame ahead of VDP1's skyline) lasted the whole quick look up, 4
# frames, so the flash check couldn't see it: each frame differed from its neighbours as much as a
# good one. What gave it away was its colour: a pink no still frame before, at the top of the look
# or after had. Mednafen draws without filtering, so a move shows the colours of the views it goes
# between. A film holds still before, between and after its moves, and those frames are the
# "known" colours; any colour beyond them in a moving frame is a fault.

COLOUR_SHIFT = 4  # colours compared in 16 steps a channel...
STRAY_MIN = 150  # ...and a frame with more samples than this (~0.2% of the screen) of a colour
# no still frame has, or one step from it (fog shades a little apart), is a fault. Measured: the
# pink band gave 4173, 2247, 853 and 239; good moving frames at most 41


def colours(frame):
    """{colour bin: samples} of a frame."""
    w, h, d = frame
    count = {}
    for y in range(0, h, STEP):
        row = y * w * 3
        for x in range(row, row + w * 3, STEP * 3):
            k = (d[x] >> COLOUR_SHIFT, d[x + 1] >> COLOUR_SHIFT, d[x + 2] >> COLOUR_SHIFT)
            count[k] = count.get(k, 0) + 1
    return count


def known_colours(rec, still_frames):
    """Every colour bin the still frames have, and the bins one step from each."""
    seen = set()
    for i in still_frames:
        seen |= set(colours(rec.frame(i)))
    near = set()
    for r, g, b in seen:
        for dr in (-1, 0, 1):
            for dg in (-1, 0, 1):
                for db in (-1, 0, 1):
                    near.add((r + dr, g + dg, b + db))
    return near


def stray_frames(rec, lo, hi, still_frames):
    """[(frame, samples, the most common stray colour as RGB)] of the moving frames in lo..hi-1."""
    known = known_colours(rec, still_frames)
    still = set(still_frames)
    found = []
    for i in range(lo, hi):
        if i in still:
            continue
        stray = {k: n for k, n in colours(rec.frame(i)).items() if k not in known}
        total = sum(stray.values())
        if total > STRAY_MIN:
            top = max(stray, key=stray.get)
            found.append((i, total, tuple(c << COLOUR_SHIFT for c in top)))
    return found


# --- The holes check: the back colour on screen -----------------------------------------------

HOLE_MIN = 20  # samples of magenta in a frame (a few stray pixels at a seam aren't a hole)


def is_magenta(r, g, b):
    return r >= 224 and g <= 32 and b >= 224


def hole_frames(rec, lo, hi):
    """[(frame, samples, first row, last row)] of the frames in lo..hi-1 showing the magenta back
    colour of a glitch-check build (src/features.h GLITCH_CHECK)."""
    found = []
    for i in range(lo, hi):
        w, h, d = rec.frame(i)
        n, top, bottom = 0, h, -1
        for y in range(0, h, STEP):
            row = y * w * 3
            for x in range(row, row + w * 3, STEP * 3):
                if is_magenta(d[x], d[x + 1], d[x + 2]):
                    n += 1
                    top, bottom = min(top, y), max(bottom, y)
        if n > HOLE_MIN:
            found.append((i, n, top, bottom))
    return found


# --- Running a check from run.sh ---------------------------------------------------------------

SAVE_MAX = 5  # faulty frames saved a check: enough to see it, not a folder of them


def save(rec, i, path):
    w, h, d = rec.frame(i)
    with open(path, "wb") as f:
        f.write(encode_png(w, h, d))


def check_flashes(rec, lo, hi, out, name):
    still_frames = stills(rec, lo, hi)
    flashes = flash_frames(rec, lo, hi)
    for n, (i, before, after, across) in enumerate(flashes):
        print("flash at frame %d: %d samples changed from the one before, %d to the one after, "
              "%d across" % (i, before, after, across))
        if n < SAVE_MAX:
            for tag, j in (("a", i - 1), ("b", i), ("c", i + 1)):
                save(rec, j, os.path.join(out, "%s_flash_%d_%s.png" % (name, n, tag)))
    if not still_frames:
        print("stray colours: not checked, no still frame (hold still before and after a move)")
        strays = []
    else:
        strays = stray_frames(rec, lo, hi, still_frames)
    for n, (i, total, rgb) in enumerate(strays):
        print("stray colour at frame %d: %d samples, mostly ~(%d, %d, %d)" % ((i, total) + rgb))
        if n < SAVE_MAX:
            save(rec, i, os.path.join(out, "%s_stray_%d.png" % (name, n)))
    print("%s: frames %d-%d, %d still: %d flashes, %d frames with stray colours" %
          (name, lo, hi - 1, len(still_frames), len(flashes), len(strays)))
    return 1 if flashes or strays else 0


def check_holes(rec, lo, hi, out, name):
    holes = hole_frames(rec, lo, hi)
    for n, (i, samples, top, bottom) in enumerate(holes):
        print("hole at frame %d: %d samples of the back colour, rows %d-%d" % (i, samples, top, bottom))
        if n < SAVE_MAX:
            save(rec, i, os.path.join(out, "%s_hole_%d.png" % (name, n)))
    print("%s: frames %d-%d: %d with holes" % (name, lo, hi - 1, len(holes)))
    return 1 if holes else 0


def main(argv):
    if len(argv) >= 3 and argv[1] == "count":
        print(len(frame_offsets(argv[2])))
        return 0
    if len(argv) >= 6 and argv[1] in ("flashes", "holes"):
        rec = Recording(argv[2])
        lo, hi = int(argv[3]), min(int(argv[4]), len(rec))
        name = argv[6] if len(argv) > 6 else "film"
        check = check_flashes if argv[1] == "flashes" else check_holes
        return check(rec, lo, hi, argv[5], name)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
