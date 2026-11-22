"""Where the CPUs spend a run: jo's PC sampling profiler (JO_TEST_PCPROF_FRAME; build with
PCPROF_FRAME=N in the environment, the makefile passes it on) read back from a Mednafen save state
(a film's savestate step), each sample put to the function of game.elf it fell in.

    python tests/e2e/pcprof.py STATE.mc0 game.elf [top]

As jo's tools/pcprof_report.py, but with the symbols read from the ELF itself (as jo's
tools/slow_calls.py does): no SH-2 binutils needed (jo's toolchain here has none). Histograms: bins of
16 bytes from 0x06004000, the master's at 0x002E0000 and the slave's at 0x002F0000 (Low Work RAM)"""
import collections, bisect, gzip, struct, sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'tools'))
from slow_calls import read_elf

BIN, BINS, BASE = 16, 16384, 0x06004000
OFFSETS = (('master', 0xE0000), ('slave', 0xF0000))

state, elf = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 30
d = gzip.open(state).read()
o = d.index(b'\x08WorkRAML') + 1 + 8 + 4
lw = d[o:o + 0x100000]
_, symbols = read_elf(elf)
funcs = sorted((a, s, n) for a, s, n in symbols if s > 0)
starts = [a for a, _, _ in funcs]


def name(a):
    i = bisect.bisect_right(starts, a) - 1
    if i >= 0 and a < funcs[i][0] + funcs[i][1]:
        n = funcs[i][2]
        return n[1:] if n.startswith('_') else n
    return '(no function)'


for cpu, off in OFFSETS:
    words = struct.unpack('<%dH' % (BINS * 2), lw[off:off + BINS * 4])
    h = [(words[2 * i] << 16) | words[2 * i + 1] for i in range(BINS)]
    total = sum(h)
    print('== %s: %d samples' % (cpu, total))
    if not total:
        continue
    by = collections.Counter()
    for i, c in enumerate(h[:-1]):
        if c:
            by[name(BASE + i * BIN)] += c
    by['(outside the code range)'] += h[-1]
    for n, c in by.most_common(top):
        print('  %5.1f %%  %7d  %s' % (100.0 * c / total, c, n))
